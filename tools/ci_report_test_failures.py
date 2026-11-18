#!/usr/bin/env python3
"""
Turn a failed Gradle job into a readable diagnosis.

Job logs are served from a results host that is not always reachable from a sandbox or a script, and
the check-run API otherwise reports only "Process completed with exit code 1". Annotations and the job
summary ARE reachable, so this reads the JUnit XML and the tee'd Gradle log locally on the runner and
re-emits the parts that matter.

Two output modes, because the two channels have different constraints:
  (default)  Markdown for $GITHUB_STEP_SUMMARY -- everything, no cap.
  --commands GitHub workflow commands. These are capped at 10 annotations per level per run, so the
             budget is spent deliberately: real test failures and compiler errors first as `error`,
             log context as `notice`. An earlier shell version listed every testcase and burned the
             entire error budget before it reported a single failure.

Usage:
    python3 tools/ci_report_test_failures.py GRADLE_LOG [--commands] [--root DIR]
"""
from __future__ import annotations

import argparse
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET

# GitHub keeps 10 annotations per level per run, so emitting more than 10 errors just drops the tail
# at random. Everything is ranked by how much it explains, then the best 10 are emitted.
MAX_ERROR_COMMANDS = 10
MAX_NOTICE_COMMANDS = 8

# Lines in a Gradle log that carry an actual cause rather than progress noise.
INTERESTING = re.compile(
    r"(?:^e: |^w: .*(?:error|deprecat)|error:|fatal error|undefined (?:reference|symbol)"
    r"|ninja: build stopped|Execution failed|What went wrong|Caused by:"
    r"|^\s*> Task .*FAILED|BUILD FAILED|tests? completed, \d+ failed"
    r"|UnsatisfiedLinkError|NoClassDefFoundError|AssertionError|ComparisonFailure)",
)


# Markers that mean the instrumentation itself died. These matter most when no JUnit XML exists at
# all, because a crashed app process writes no results and "no failures found" would otherwise read as
# a pass. Each is reported with the line that carried it.
CRASH_MARKERS = re.compile(
    r"|".join([
        r"Process crashed", r"am_crash", r"SIGSEGV", r"SIGABRT", r"Fatal signal",
        r"OutOfMemoryError", r"Test run failed to complete", r"Instrumentation run failed",
        r"failed to complete", r"Killed", r"app_process died", r"ANR in",
    ])
)

# Gradle's own internal frames carry no information about our failure and, at up to 10 annotations per
# level per run, crowd out the ones that do. They are sorted last rather than dropped.
GRADLE_INTERNAL_FRAME = re.compile(r"^\s*(at|Caused by:)?\s*org\.gradle\.")


def collect_crash_lines(log_path: str) -> list[str]:
    """Lines showing the instrumentation or app process dying, rather than a test assertion failing."""
    if not os.path.isfile(log_path):
        return []
    with open(log_path, "r", errors="replace") as handle:
        found, seen = [], set()
        for line in handle:
            line = line.strip()
            if line and CRASH_MARKERS.search(line) and line[:160] not in seen:
                seen.add(line[:160])
                found.append(line)
    return found


# Header lines that begin a crash block in logcat. The reason a process died is never on the header
# line itself: a Java crash puts its stack on the lines that follow "FATAL EXCEPTION", and a native
# crash puts its abort message and backtrace after "Fatal signal". So the block is what matters.
# Only lines that genuinely BEGIN a crash. "Abort message:" and "backtrace:" are deliberately absent:
# listing them here fragmented one native crash into several blocks, and the block cap then discarded
# the backtrace - the part that names the faulting library. They are captured as context inside the
# block that "Fatal signal" opens, and ranked by CRASH_KEY_LINE when emitted.
CRASH_BLOCK_HEADERS = re.compile(
    r"FATAL EXCEPTION|Fatal signal|beginning of crash|Process crashed|am_crash|ANR in "
)

# "lowmemorykiller: lmkd data connection established" and "ActivityManager: Killing
# 1900:com.google.android.sdksetup" are boot noise that a real run's logcat is full of. Treating them
# as crash headers filled every annotation slot with lines about other processes while the actual
# death went unreported. A kill only matters if it is OUR process being killed.
OUR_PACKAGE = re.compile(r"com\.davemega")
KILL_LINE = re.compile(
    r"lowmemorykiller|lmkd|ActivityManager: Killing|am_kill|has died|Process .* has died|Killing "
    r"|Force finishing|Scheduling restart"
)


# Our package being killed is not always a crash. Android tears the app down when instrumentation
# finishes, and that line names our process too:
#   ActivityManager: Killing 6110:com.davemega.hub/u0a167 (adj 0): stop com.davemega.hub due to
#   finished inst
# Reporting it put a failure-level annotation on a run whose three tests had all passed.
BENIGN_KILL = re.compile(
    r"due to finished inst|finished inst|due to stop|due to empty"
    r"|due to deletePackageX|deletePackageX|due to remove task|too many empty"
)


def collect_kill_lines(paths: list[str]) -> list[str]:
    """Lines showing OUR process being killed or dying, as opposed to any process on the device."""
    found: list[str] = []
    seen: set[str] = set()
    for path in paths:
        if not path or not os.path.isfile(path):
            continue
        with open(path, "r", errors="replace") as handle:
            for line in handle:
                line = line.strip()
                if not line or line[:160] in seen:
                    continue
                if KILL_LINE.search(line) and OUR_PACKAGE.search(line) and not BENIGN_KILL.search(line):
                    seen.add(line[:160])
                    found.append(line)
    return found
# Lines inside a crash block that actually explain the death, as opposed to the context around it. A
# 10-annotation cap means a Java stack of "at ..." frames can otherwise crowd out the native abort
# message and backtrace that name the real fault.
CRASH_KEY_LINE = re.compile(
    r"FATAL EXCEPTION|Fatal signal|Abort message|backtrace:|Caused by|Exception|Error"
    r"|com\.davemega|libafcore|libfilament|SIGSEGV|SIGABRT|lowmemorykiller|Killing"
    r"|Process crashed|am_crash|ANR in"
)

CRASH_BLOCK_CONTEXT = 30
CRASH_BLOCK_MAX = 4


def collect_crash_blocks(paths: list[str]) -> list[tuple[str, list[str]]]:
    """Extracts crash blocks from logcat-style files, as (path, lines).

    The Gradle log says only "Instrumentation run failed due to Process crashed." The reason is in
    logcat, which the job captures but nothing read - so a crashed run was still undiagnosable even
    after the totals and crash markers were surfaced.
    """
    blocks: list[tuple[str, list[str]]] = []
    for path in paths:
        if not path or not os.path.isfile(path):
            continue
        with open(path, "r", errors="replace") as handle:
            lines = [line.rstrip("\n") for line in handle]
        for index, line in enumerate(lines):
            if not CRASH_BLOCK_HEADERS.search(line):
                continue
            block = [line.strip()]
            for following in lines[index + 1:index + 1 + CRASH_BLOCK_CONTEXT]:
                stripped = following.strip()
                if not stripped:
                    continue
                # Stop at the next crash header so one block does not swallow the following one.
                if CRASH_BLOCK_HEADERS.search(stripped):
                    break
                block.append(stripped)
            blocks.append((path, merge_abort_messages(block)))
            if len(blocks) >= CRASH_BLOCK_MAX:
                return blocks
    return blocks


ABORT_MESSAGE = re.compile(r"Abort message:")
# A tombstone field that ends the abort message. The message itself is a quoted C++ string, so the
# closing quote is the reliable terminator; these are the fields that follow it when the quote is
# missing from a truncated capture.
TOMBSTONE_FIELD = re.compile(
    r"\b(signal \d+|backtrace:|pid:|uid:|Timestamp:|ABI:|Revision:|Build fingerprint:)\b"
)


def merge_abort_messages(block: list[str]) -> list[str]:
    """Joins a tombstone's abort message, which wraps across logcat lines, into one line.

    The message is the single most useful thing a native crash carries - it is the assertion that
    failed, in the crashing library's own words. Captured one logcat line at a time it reads
    "terminating due to uncaught exception of type utils::PreconditionPanic: Precondition" and stops
    exactly where the useful part begins: the file, line and condition are on the next line. Emitting
    the halves separately lost the second to the annotation cap.
    """
    merged: list[str] = []
    index = 0
    while index < len(block):
        line = block[index]
        if not ABORT_MESSAGE.search(line):
            merged.append(line)
            index += 1
            continue
        parts = [line]
        index += 1
        # Continuation lines until the closing quote of the message, a new tombstone field, or a
        # short bound so a malformed capture cannot swallow the whole block.
        while index < len(block) and len(parts) < 6:
            following = block[index]
            if ABORT_MESSAGE.search(following) or TOMBSTONE_FIELD.search(following):
                break
            parts.append(following)
            index += 1
            if following.rstrip().endswith("'"):
                break
        merged.append(" ".join(part.strip() for part in parts))
    return merged


def escape_command(text: str) -> str:
    """Escape a value for a workflow command. Unescaped % and newlines silently corrupt annotations."""
    return (
        text.replace("%", "%25")
        .replace("\r", "%0D")
        .replace("\n", "%0A")
        .replace("\x00", "")
    )


def collect_test_failures(root: str) -> list[tuple[str, str, str, str]]:
    """Returns (kind, classname, name, detail) for every failed or errored test."""
    out: list[tuple[str, str, str, str]] = []
    patterns = [
        os.path.join(root, "*/build/test-results/*/*.xml"),
        os.path.join(root, "build/test-results/*/*.xml"),
        # connectedDebugAndroidTest writes here, not under build/test-results. Without these the
        # instrumented job reported "No JUnit failures found" for a run that had just failed, because
        # the only XML it looked at was the JVM unit-test output from a different job.
        os.path.join(root, "*/build/outputs/androidTest-results/connected/**/*.xml"),
        os.path.join(root, "*/build/outputs/androidTest-results/connected/*.xml"),
        os.path.join(root, "build/outputs/androidTest-results/connected/**/*.xml"),
    ]
    files = sorted({f for pattern in patterns for f in glob.glob(pattern, recursive=True)})
    for path in files:
        try:
            tree = ET.parse(path)
        except ET.ParseError as exc:
            out.append(("xml-error", path, "", str(exc)))
            continue
        for testcase in tree.iter("testcase"):
            for kind in ("failure", "error"):
                element = testcase.find(kind)
                if element is None:
                    continue
                message = (element.get("message") or "").strip()
                body = (element.text or "").strip()
                lines = body.splitlines()
                detail = message or (lines[0].strip() if lines else "")
                # The first frame in our own code says where to look. JUnit writes it as "at pkg...",
                # so strip the leading "at" rather than printing "at at".
                ours = [line.strip() for line in lines if "com.davemega" in line]
                if ours:
                    frame = ours[0][3:].strip() if ours[0].startswith("at ") else ours[0]
                    detail = f"{detail} | at {frame}" if detail else f"at {frame}"
                out.append((
                    kind,
                    testcase.get("classname") or "?",
                    testcase.get("name") or "?",
                    detail[:900],
                ))
    return out


def collect_test_totals(root: str) -> tuple[int, int, int, int, list[str]]:
    """Returns (tests, failures, errors, skipped, classes) summed over every JUnit XML found.

    Read from the testsuite attributes rather than by counting testcase elements: a suite that crashed
    mid-run can list fewer testcases than it started, and the difference between "ran and passed" and
    "never ran" is exactly what a CI gate has to be able to tell apart.
    """
    patterns = [
        os.path.join(root, "*/build/test-results/*/*.xml"),
        os.path.join(root, "build/test-results/*/*.xml"),
        os.path.join(root, "*/build/outputs/androidTest-results/connected/**/*.xml"),
        os.path.join(root, "*/build/outputs/androidTest-results/connected/*.xml"),
        os.path.join(root, "build/outputs/androidTest-results/connected/**/*.xml"),
    ]
    files = sorted({f for pattern in patterns for f in glob.glob(pattern, recursive=True)})
    tests = failures = errors = skipped = 0
    classes: list[str] = []
    for path in files:
        try:
            tree = ET.parse(path)
        except ET.ParseError:
            continue
        for suite in tree.iter("testsuite"):
            def num(name: str) -> int:
                try:
                    return int(suite.get(name) or 0)
                except ValueError:
                    return 0
            tests += num("tests")
            failures += num("failures")
            errors += num("errors")
            skipped += num("skipped")
            name = suite.get("name")
            if name:
                classes.append(name)
            # A connected run can put every test under one <testsuite> named after whichever class the
            # runner started with, so suite names alone reported "classes=...SmokeTest" for a run that
            # had executed seven tests across two classes. Which classes actually ran is the single
            # thing this report exists to prove, so it is taken from the testcases as well.
            for case in suite.iter("testcase"):
                case_class = case.get("classname")
                if case_class:
                    classes.append(case_class)
    return (tests, failures, errors, skipped, sorted(set(classes)))


def collect_log_lines(log_path: str) -> tuple[list[str], list[str]]:
    """Returns (interesting lines, tail). Missing log is reported, not silently ignored."""
    if not os.path.isfile(log_path):
        return ([f"{log_path} does not exist -- the Gradle step never wrote a log"], [])
    with open(log_path, "r", errors="replace") as handle:
        lines = [line.rstrip() for line in handle]
    if not lines:
        return ([f"{log_path} is empty"], [])
    interesting = [line for line in lines if INTERESTING.search(line)]
    # De-duplicate while keeping order: Gradle repeats the same error per variant.
    seen: set[str] = set()
    unique = []
    for line in interesting:
        key = line.strip()[:200]
        if key not in seen:
            seen.add(key)
            unique.append(line.strip())
    return (unique, [line for line in lines[-30:] if line.strip()])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("gradle_log", help="tee'd Gradle output for the failed step")
    parser.add_argument("--commands", action="store_true",
                        help="emit GitHub workflow commands instead of Markdown")
    parser.add_argument("--root", default=".", help="repository root to search for test-results")
    parser.add_argument("--crash-log", action="append", default=[],
                        help="extra log to scan for process-death blocks, typically the pulled "
                             "logcat. Repeatable. The Gradle log only says a process crashed; "
                             "logcat says why.")
    parser.add_argument("--counts", action="store_true",
                        help="print one machine-readable line, 'tests=N failures=F errors=E skipped=S "
                             "classes=A,B', and exit non-zero if any test failed or none ran. For CI "
                             "gates that must not hard-code an expected test count in a grep.")
    args = parser.parse_args()

    if args.counts:
        tests, failures, errors, skipped, classes = collect_test_totals(args.root)
        print("tests=%d failures=%d errors=%d skipped=%d classes=%s"
              % (tests, failures, errors, skipped, ",".join(classes)))
        if tests == 0:
            print("no JUnit XML was found: the tests never ran, or the results path changed",
                  file=sys.stderr)
            return 2
        if failures or errors:
            return 1
        return 0

    failures = collect_test_failures(args.root)
    log_lines, tail = collect_log_lines(args.gradle_log)

    crashes = collect_crash_lines(args.gradle_log)

    if args.commands:
        # Everything competes for the same 10 annotation slots, so candidates are collected with a
        # priority and the most explanatory ones are emitted. Emitting in discovery order let a long
        # Java stack push out the native abort message that named the actual fault.
        # (priority, level, title, message). Level matters as much as priority: GitHub caps 10
        # annotations PER LEVEL per run, so demoting context to ::notice stops it competing with the
        # lines that explain the failure for the same 10 ::error slots.
        candidates: list[tuple[int, str, str, str]] = []

        # 0: totals, always. Without this a run that wrote no XML and a run whose tests all passed
        # produce identical annotations - which is how a suite that stopped running hides.
        ran, failed_total, errored, skipped_total, classes = collect_test_totals(args.root)
        totals = ("tests=%d failures=%d errors=%d skipped=%d classes=%s"
                  % (ran, failed_total, errored, skipped_total, ",".join(classes)))
        if ran == 0:
            candidates.append((0, "error", "test-totals",
                               "no JUnit XML was found under %s, so nothing proves any test ran (%s)"
                               % (args.root, totals)))
        elif failed_total or errored:
            candidates.append((0, "error", "test-totals", totals))
        else:
            candidates.append((0, "notice", "test-totals", totals))

        # 1: a test's own assertion message and the first frame in our code.
        for kind, classname, name, detail in failures:
            candidates.append((1, "error", f"{kind}: {classname}.{name}", detail))

        # 2: why a process died. The Gradle log's own markers, then the lines of each logcat crash
        #    block that actually name a fault. 3: the rest of that block, which is context.
        for line in crashes:
            candidates.append((2, "error", "process-crash", line))
        for path, block in collect_crash_blocks(args.crash_log):
            for line in block:
                label = "%s: %s" % (os.path.basename(path), line)
                if CRASH_KEY_LINE.search(line):
                    candidates.append((2, "error", "crash-log", label))
                else:
                    candidates.append((3, "notice", "crash-log", label))

        # 4: generic Gradle output, our own lines before its internal plumbing. Gradle's own
        #    TaskExecutionException plumbing is context, not cause, so it is demoted to a notice.
        for line in sorted(log_lines, key=lambda item: bool(GRADLE_INTERNAL_FRAME.match(item))):
            level = "notice" if GRADLE_INTERNAL_FRAME.match(line) else "error"
            candidates.append((4, level, "gradle", line))

        # 2: our own process being killed. This outranks generic crash context because it is the only
        #    line in a device logcat that is definitely about us - but it is a CAUSE only if the run
        #    actually failed. Android tears our process down at the end of every instrumentation run and
        #    that teardown names our package too, so the level is decided by the totals rather than by
        #    recognising the reason. Enumerating benign reasons is a losing game: "due to finished inst"
        #    (run 44233306427) and then "due to deletePackageX" (run 44240305554) each reached here as a
        #    failure-level annotation on a run whose tests had all passed, and the second got through the
        #    filter added for the first. On a green run the line is still reported, as a notice: it is
        #    true and it is simply not the reason for anything.
        green_run = bool(ran) and (failed_total + errored) == 0
        for line in collect_kill_lines(args.crash_log):
            if green_run:
                candidates.append((2, "notice", "process-killed-after-success", line))
            else:
                candidates.append((2, "error", "process-killed", line))

        # 1: a suite that counts failures but recorded no message for some of them. Raised from last
        #    to first among the details: it is the strongest available signal that a test died mid-run
        #    rather than asserting false, and at the bottom of the queue the annotation cap ate it.
        if ran and (failed_total + errored) > len(failures):
            candidates.append((1, "error", "test-totals",
                               "%d failure(s) counted by the result files but only %d carried a "
                               "message, so the rest died before recording one (a crash or a kill, "
                               "not an assertion)" % (failed_total + errored, len(failures))))

        candidates.sort(key=lambda item: item[0])
        budget = {"error": MAX_ERROR_COMMANDS, "notice": MAX_NOTICE_COMMANDS}
        emitted = 0
        for _priority, level, title, message in candidates:
            if budget[level] <= 0:
                continue
            budget[level] -= 1
            emitted += 1
            print("::%s title=%s::%s" % (level, escape_command(title), escape_command(message[:900])))
        if emitted == 0:
            print("::error::no test results, no crash marker and no recognisable Gradle error were found")
        # The log tail is the lowest-value material there is, and it only gets whatever notice budget
        # the crash context and Gradle plumbing left over.
        for line in [item for item in tail if not GRADLE_INTERNAL_FRAME.match(item)]:
            if budget["notice"] <= 0:
                break
            budget["notice"] -= 1
            print(f"::notice title=log-tail::{escape_command(line[:900])}")
        return 0

    print("## Gradle failure report")
    print()
    print(f"Log: `{args.gradle_log}`")
    print()
    ran, failed_total, errored, skipped_total, classes = collect_test_totals(args.root)
    print("### Result totals")
    print()
    print("```")
    print("tests=%d failures=%d errors=%d skipped=%d" % (ran, failed_total, errored, skipped_total))
    print("classes=%s" % (", ".join(classes) or "<none found>"))
    print("```")
    print()
    if ran == 0:
        print("> **No JUnit XML was found.** Nothing proves any test ran.")
        print()
    if crashes:
        print("### Process or instrumentation died")
        print()
        for line in crashes[:MAX_ERROR_COMMANDS]:
            print(f"- `{line[:900]}`")
        print()
    for path, block in collect_crash_blocks(args.crash_log):
        print(f"### Crash block from `{os.path.basename(path)}`")
        print()
        print("```")
        for line in block[:CRASH_BLOCK_CONTEXT]:
            print(line[:900])
        print("```")
        print()
    kills = collect_kill_lines(args.crash_log)
    if kills:
        print("### Our process was killed")
        print()
        for line in kills[:MAX_ERROR_COMMANDS]:
            print(f"- `{line[:900]}`")
        print()
    if ran and (failed_total + errored) > len(failures):
        print(f"> **{failed_total + errored} failure(s) were counted but only {len(failures)} carried a "
              "message.** The others died before recording one.")
        print()
    if failures:
        print(f"### {len(failures)} failed test(s)")
        print()
        for kind, classname, name, detail in failures:
            print(f"- **{classname}.{name}** ({kind})")
            print(f"  - `{detail}`")
        print()
    else:
        print("### No JUnit failures found")
        print()
        print("Either the tests never ran (a compilation failure) or the results XML was not written.")
        print()
    if log_lines:
        print(f"### {len(log_lines)} notable log line(s)")
        print()
        for line in log_lines[:60]:
            print(f"- `{line[:400]}`")
        print()
    if tail:
        print("### Log tail")
        print()
        print("```")
        for line in tail:
            print(line[:400])
        print("```")
    return 0


if __name__ == "__main__":
    sys.exit(main())

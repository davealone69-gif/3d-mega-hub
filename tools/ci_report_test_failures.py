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

MAX_ERROR_COMMANDS = 8
MAX_NOTICE_COMMANDS = 8

# Lines in a Gradle log that carry an actual cause rather than progress noise.
INTERESTING = re.compile(
    r"(?:^e: |^w: .*(?:error|deprecat)|error:|fatal error|undefined (?:reference|symbol)"
    r"|ninja: build stopped|Execution failed|What went wrong|Caused by:"
    r"|^\s*> Task .*FAILED|BUILD FAILED|tests? completed, \d+ failed"
    r"|UnsatisfiedLinkError|NoClassDefFoundError|AssertionError|ComparisonFailure)",
)


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

    if args.commands:
        errors = 0
        for kind, classname, name, detail in failures:
            if errors >= MAX_ERROR_COMMANDS:
                break
            title = escape_command(f"{kind}: {classname}.{name}")
            print(f"::error title={title}::{escape_command(detail)}")
            errors += 1
        for line in log_lines:
            if errors >= MAX_ERROR_COMMANDS:
                break
            print(f"::error title=gradle::{escape_command(line[:900])}")
            errors += 1
        if not failures and not log_lines:
            print("::error::no test failures and no recognisable Gradle error were found")
        notices = 0
        for line in tail:
            if notices >= MAX_NOTICE_COMMANDS:
                break
            print(f"::notice title=log-tail::{escape_command(line[:900])}")
            notices += 1
        return 0

    print("## Gradle failure report")
    print()
    print(f"Log: `{args.gradle_log}`")
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

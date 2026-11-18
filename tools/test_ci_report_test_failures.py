#!/usr/bin/env python3
"""Synthetic regression tests for ci_report_test_failures.py.

The reporter exists because job logs and artifacts are served from hosts a sandbox cannot reach, so the
only programmatically readable diagnosis of a red CI job is the annotations and step summary this tool
emits. That makes it load-bearing for every Gradle job in the repository, and it is easy to break in a
way that fails silently: a glob that stops matching produces "No JUnit failures found" for a run that
just failed, which reads like a pass.

These tests therefore assert on discovery, not only on parsing. The connected-androidTest case is here
because it was genuinely broken: the tool globbed only */build/test-results/*/*.xml while
connectedDebugAndroidTest writes to app/build/outputs/androidTest-results/connected/<device>/, so the
instrumented renderer job could never have been diagnosed with it.

Run:  python3 tools/test_ci_report_test_failures.py
"""
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("ci_report_test_failures.py")
spec = importlib.util.spec_from_file_location("ci_report_test_failures", MODULE_PATH)
reporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reporter)

CONNECTED = "app/build/outputs/androidTest-results/connected/pixel_2/TEST-emulator.xml"
JVM = "app/build/test-results/testDebugUnitTest/TEST-com.davemega.hub.ClipMappingTest.xml"

FAILING_SUITE = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="com.davemega.hub.AvatarForgeRendererSmokeTest" tests="3" failures="1" errors="0"
           skipped="0" time="412.7">
  <testcase name="importedBundledGlbUsesAvatarForgeGlesAndProducesVisibleGeometry"
            classname="com.davemega.hub.AvatarForgeRendererSmokeTest" time="88.2"/>
  <testcase name="requiredWebpTextureExtensionIsDecodedAndRenderedByAvatarForge"
            classname="com.davemega.hub.AvatarForgeRendererSmokeTest" time="61.0"/>
  <testcase name="generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels"
            classname="com.davemega.hub.AvatarForgeRendererSmokeTest" time="263.5">
    <failure message="Generated avatar had fewer than three successful EGL swaps"
             type="junit.framework.AssertionFailedError">junit.framework.AssertionFailedError: Generated avatar had fewer than three successful EGL swaps
\tat org.junit.Assert.fail(Assert.java:89)
\tat com.davemega.hub.AvatarForgeRendererSmokeTest.generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels(AvatarForgeRendererSmokeTest.kt:114)
</failure>
  </testcase>
</testsuite>
"""

PASSING_SUITE = FAILING_SUITE.replace('failures="1"', 'failures="0"').split("<testcase name=\"generatedAvatar")[0] + \
    '<testcase name="generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels" ' \
    'classname="com.davemega.hub.AvatarForgeRendererSmokeTest" time="263.5"/>\n</testsuite>\n'

JVM_SUITE = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="com.davemega.hub.ClipMappingTest" tests="2" failures="0" errors="1" skipped="0">
  <testcase name="completeRequiresEveryIndexResolved" classname="com.davemega.hub.ClipMappingTest">
    <error message="boom" type="java.lang.IllegalStateException">java.lang.IllegalStateException: boom
\tat com.davemega.hub.ClipMappingTest.completeRequiresEveryIndexResolved(ClipMappingTest.kt:22)
</error>
  </testcase>
  <testcase name="emptyMappingIsNotComplete" classname="com.davemega.hub.ClipMappingTest"/>
</testsuite>
"""


def write(root: Path, rel: str, body: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")


def counts_exit(root: Path) -> int:
    """Runs the tool the way the CI script does and returns its exit status."""
    proc = subprocess.run(
        [sys.executable, str(MODULE_PATH), str(root / "missing.log"), "--root", str(root), "--counts"],
        capture_output=True, text=True,
    )
    return proc.returncode


GRADLE_LOG = """> Task :app:connectedDebugAndroidTest
Starting 3 tests on Pixel_2_API_30(AVD) - 11
Tests on Pixel_2_API_30(AVD) - 11: Failed
Process crashed: com.davemega.hub.test
java.lang.OutOfMemoryError: Failed to allocate 33554432 byte allocation
\tat org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
\tat org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.call(EventFiringTaskExecuter.java:52)
BUILD FAILED in 11m 42s
"""


def commands(root: Path, log: str) -> list[str]:
    """Runs the tool in --commands mode and returns the annotation lines it emits."""
    log_path = root / "gradle.log"
    log_path.write_text(log, encoding="utf-8")
    proc = subprocess.run(
        [sys.executable, str(MODULE_PATH), str(log_path), "--root", str(root), "--commands"],
        capture_output=True, text=True,
    )
    return proc.stdout.splitlines()


def titles(lines: list[str]) -> list[str]:
    out = []
    for line in lines:
        if "title=" in line:
            out.append(line.split("title=", 1)[1].split("::", 1)[0])
    return out


LOGCAT = """11-18 03:12:44.101  1234  1234 I MegaHubRenderer: Renderer status: GLB parsed
11-18 03:12:45.220  1234  1290 E AndroidRuntime: FATAL EXCEPTION: MegaHubGlbReader
11-18 03:12:45.221  1234  1290 E AndroidRuntime: Process: com.davemega.hub, PID: 1234
11-18 03:12:45.222  1234  1290 E AndroidRuntime: java.lang.OutOfMemoryError: Failed to allocate 83642456 byte allocation
11-18 03:12:45.223  1234  1290 E AndroidRuntime: \tat com.davemega.hub.FilamentPreviewView.readBounded(FilamentPreviewView.kt:648)
11-18 03:12:46.900  1234  1234 I MegaHubRenderer: unrelated line after the crash
11-18 03:12:50.010   999   999 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** ***
11-18 03:12:50.011   999   999 F DEBUG   : Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0 in tid 1290
11-18 03:12:50.012   999   999 F DEBUG   : Abort message: 'filament: out of memory allocating vertex buffer'
11-18 03:12:50.020   999   999 F DEBUG   : backtrace:
11-18 03:12:50.021   999   999 F DEBUG   :       #00 pc 0000000000123456  /data/app/lib/x86_64/libafcore.so (af::importGlb+102)
11-18 03:12:51.000  1234  1234 I MegaHubRenderer: nothing to see here
"""


def _write(root: Path, name: str, body: str) -> Path:
    path = root / name
    path.write_text(body, encoding="utf-8")
    return path


def commands_with_logcat(root: Path, log: str, logcat: str) -> list[str]:
    log_path = root / "gradle.log"
    log_path.write_text(log, encoding="utf-8")
    cat_path = root / "logcat.txt"
    cat_path.write_text(logcat, encoding="utf-8")
    proc = subprocess.run(
        [sys.executable, str(MODULE_PATH), str(log_path), "--root", str(root),
         "--crash-log", str(cat_path), "--commands"],
        capture_output=True, text=True,
    )
    return proc.stdout.splitlines()


def main() -> None:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)

        # ---- 1. the regression that mattered: connected instrumented results must be found ----
        write(root, CONNECTED, FAILING_SUITE)
        failures = reporter.collect_test_failures(str(root))
        assert len(failures) == 1, f"expected the connected failure, got {failures}"
        kind, classname, name, detail = failures[0]
        assert kind == "failure", kind
        assert classname == "com.davemega.hub.AvatarForgeRendererSmokeTest", classname
        assert name == "generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels", name

        # ---- 2. the detail must carry the assertion AND the first frame in our own code ----
        assert "fewer than three successful EGL swaps" in detail, detail
        assert "AvatarForgeRendererSmokeTest.kt:114" in detail, detail
        # ...and must not waste the annotation budget on JUnit's own internals.
        assert "org.junit.Assert.fail" not in detail, detail

        # ---- 3. totals come from the testsuite attributes, so a truncated run is visible ----
        tests, failed, errors, skipped, classes = reporter.collect_test_totals(str(root))
        assert (tests, failed, errors, skipped) == (3, 1, 0, 0), (tests, failed, errors, skipped)
        assert classes == ["com.davemega.hub.AvatarForgeRendererSmokeTest"], classes

        # ---- 4. exit statuses: the CI gate branches on these, so all three must be distinct ----
        assert counts_exit(root) == 1, "a failing suite must exit 1"
        write(root, CONNECTED, PASSING_SUITE)
        tests, failed, errors, skipped, _ = reporter.collect_test_totals(str(root))
        assert (tests, failed, errors) == (3, 0, 0), (tests, failed, errors)
        assert counts_exit(root) == 0, "a passing suite must exit 0"

        # ---- 5. JVM results must still be found: the unit-test and APK jobs depend on them ----
        write(root, JVM, JVM_SUITE)
        failures = reporter.collect_test_failures(str(root))
        assert len(failures) == 1, f"the JVM error must also be reported, got {failures}"
        assert failures[0][0] == "error", failures[0]
        assert "ClipMappingTest.kt:22" in failures[0][3], failures[0]
        tests, failed, errors, skipped, classes = reporter.collect_test_totals(str(root))
        assert (tests, failed, errors) == (5, 0, 1), (tests, failed, errors)
        assert len(classes) == 2, classes

    # ---- 6. "never ran" must not look like "passed" ----
    with tempfile.TemporaryDirectory() as td:
        assert counts_exit(Path(td)) == 2, "an empty tree must exit 2, not 0"
        assert reporter.collect_test_failures(td) == []

    # ---- 7. malformed XML must be reported, not swallowed ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, "<testsuite><testcase name='x'")
        failures = reporter.collect_test_failures(str(root))
        assert failures and failures[0][0] == "xml-error", failures
        # A file that cannot be parsed yields no totals, so the gate must say "never ran".
        assert counts_exit(root) == 2, "unparseable results must not pass the gate"

    # ---- 8. totals are always annotated, so "never ran" cannot look like "passed" ----
    # This is the exact blind spot a real run exposed: Gradle failed, no JUnit XML was discoverable,
    # and the annotations said nothing about it.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        lines = commands(root, GRADLE_LOG)
        assert titles(lines)[0] == "test-totals", titles(lines)
        assert lines[0].startswith("::error"), lines[0]
        assert "no JUnit XML was found" in lines[0], lines[0]
        assert "tests=0" in lines[0], lines[0]
        # A dead process is the reason there is no XML, so it must be reported too.
        assert "process-crash" in titles(lines), titles(lines)
        crashed = [ln for ln in lines if "title=process-crash" in ln]
        assert any("Process crashed" in ln for ln in crashed), crashed
        assert any("OutOfMemoryError" in ln for ln in crashed), crashed
        # Gradle's own internal frames say nothing about our failure and there are only 10 annotation
        # slots per level per run, so they must not crowd out the ones that do.
        assert not any("DefaultBuildOperationRunner" in ln for ln in lines), lines
        assert "BUILD FAILED in 11m 42s" in "\n".join(lines), lines

    # ---- 9. with results present, the totals annotation reports them instead ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, PASSING_SUITE)
        lines = commands(root, GRADLE_LOG)
        assert lines[0].startswith("::notice"), lines[0]
        assert "tests=3 failures=0" in lines[0], lines[0]
        write(root, CONNECTED, FAILING_SUITE)
        lines = commands(root, GRADLE_LOG)
        assert lines[0].startswith("::error"), lines[0]
        assert "tests=3 failures=1" in lines[0], lines[0]
        # ...and the failing test still gets its own annotation with its own message.
        named = [ln for ln in lines if "generatedAvatarDebugDiagnostic" in ln]
        assert named and "fewer than three successful EGL swaps" in named[0], named

    # ---- 10. a clean log must not invent a crash ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, PASSING_SUITE)
        lines = commands(root, "> Task :app:connectedDebugAndroidTest\nBUILD SUCCESSFUL in 4m 1s\n")
        assert not any("process-crash" in ln for ln in lines), lines
        assert not any("no JUnit XML" in ln for ln in lines), lines

    # ---- 11. logcat is where the reason lives; the Gradle log only says a process crashed ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        lines = commands_with_logcat(
            root, "Test run failed to complete. Instrumentation run failed due to Process crashed.\n",
            LOGCAT)
        joined = "\n".join(lines)
        assert "Abort message" in joined, joined
        assert "out of memory allocating vertex buffer" in joined, joined
        assert "Fatal signal 11 (SIGSEGV)" in joined, joined
        # The frame that names the faulting library is the single most useful line in a native crash.
        assert "libafcore.so" in joined, joined
        assert "af::importGlb" in joined, joined

    # ---- 12. a crash block must not be fragmented by its own interior lines ----
    # "Abort message:" and "backtrace:" used to count as block headers, so one native crash became
    # three blocks and the block cap threw away the backtrace.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        blocks = reporter.collect_crash_blocks([str(_write(root, "logcat.txt", LOGCAT))])
        assert len(blocks) == 2, [b[1][0] for b in blocks]
        native = blocks[1][1]
        assert any("Fatal signal" in ln for ln in native), native
        assert any("Abort message" in ln for ln in native), native
        assert any("backtrace:" in ln for ln in native), native
        assert any("libafcore.so" in ln for ln in native), native
        # The Java block keeps its own stack, and stops before the native crash begins.
        java = blocks[0][1]
        assert any("FATAL EXCEPTION" in ln for ln in java), java
        assert any("readBounded" in ln for ln in java), java
        assert not any("Fatal signal" in ln for ln in java), java

    # ---- 13. cause lines outrank context inside the 10-error cap ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        lines = commands_with_logcat(root, "BUILD FAILED in 6m 48s\n", LOGCAT)
        errors = [ln for ln in lines if ln.startswith("::error")]
        notices = [ln for ln in lines if ln.startswith("::notice")]
        assert len(errors) <= 10, len(errors)
        # The abort message must survive even though a Java stack precedes it.
        assert any("Abort message" in ln for ln in errors), errors
        assert any("libafcore.so" in ln for ln in errors), errors
        # Filler is demoted rather than dropped, so the notice budget carries it.
        assert any("unrelated line after the crash" in ln for ln in notices), notices
        assert any("*** ***" in ln for ln in notices), notices
        assert not any("unrelated line after the crash" in ln for ln in errors), errors

    # ---- 14. a passing run must not be annotated as an error ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, PASSING_SUITE)
        lines = commands_with_logcat(root, "BUILD SUCCESSFUL in 4m 1s\n", "")
        assert lines[0].startswith("::notice title=test-totals"), lines[0]
        assert not any(ln.startswith("::error") for ln in lines), lines

    print("PASS: connected + JVM discovery, failure detail, totals, the 0/1/2 gate statuses, "
          "always-annotated totals, crash detection, Gradle-frame deprioritisation, logcat crash "
          "blocks, block integrity, cause-over-context ranking, and per-level budgets")


if __name__ == "__main__":
    main()

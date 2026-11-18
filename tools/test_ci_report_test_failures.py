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

    print("PASS: connected + JVM discovery, failure detail, totals, and the 0/1/2 gate exit statuses")


if __name__ == "__main__":
    main()

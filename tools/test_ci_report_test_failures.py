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


# Real lines from run 44227951941's logcat. Every one of these was reported as a crash block and
# consumed the annotation budget, while the actual death of our process went unreported.
BOOT_NOISE_LOGCAT = """11-18 03:07:56.217   160   160 I lowmemorykiller: Using psi monitors for memory pressure detection
11-18 03:07:56.217   160   160 I lowmemorykiller: Process polling is supported
11-18 03:08:05.028   160   160 I lowmemorykiller: lmkd data connection established
11-18 03:08:22.178   492   550 I ActivityManager: Killing 1900:com.google.android.sdksetup/u0a151 (adj 0): change com.google.android.sdksetup
11-18 03:08:22.230   492   492 W AppWidgetServiceImpl: Failed to read state: java.io.FileNotFoundException: /data/system/users/0/appwidgets.xml
11-18 03:09:10.000  2100  2100 I MegaHubRenderer: Renderer status: GLB parsed
"""

KILLED_LOGCAT = BOOT_NOISE_LOGCAT + """11-18 03:11:02.440   492   550 I ActivityManager: Killing 2431:com.davemega.hub/u0a152 (adj 900): empty #17
11-18 03:11:02.450   160   160 I lowmemorykiller: Kill 'com.davemega.hub' (2431), uid 10152, oom_score_adj 900 to free 81234kB rss
"""

# Modelled on the tombstone from run 44229690337. The header lines, the pid/uid/signal lines and the
# two libfilament-jni.so frames are copied from that run's annotations. The abort message's SECOND line
# is synthetic: that run captured only "Abort message: 'terminating due to uncaught exception of type
# utils::PreconditionPanic: Precondition" and the annotation cap dropped whatever followed, which is the
# defect this fixture pins. The wording stands in for the file, line and condition a real Filament
# precondition failure puts there; it is not a quotation from that run.
TOMBSTONE_LOGCAT = """11-18 03:31:22.752  6078  6078 F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE) in tid 6078 (om.davemega.hub), pid 6078 (om.davemega.hub)
11-18 03:31:22.819  6376  6376 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
11-18 03:31:22.819  6376  6376 F DEBUG   : Build fingerprint: 'google/sdk_gphone_x86_64/generic_x86_64_arm64:11/RSR1.240422.006/12134477:userdebug/dev-keys'
11-18 03:31:22.819  6376  6376 F DEBUG   : Revision: '0'
11-18 03:31:22.819  6376  6376 F DEBUG   : ABI: 'x86_64'
11-18 03:31:22.819  6376  6376 F DEBUG   : Timestamp: 2026-11-18 03:31:22+0000
11-18 03:31:22.819  6376  6376 F DEBUG   : pid: 6078, tid: 6078, name: om.davemega.hub  >>> com.davemega.hub <<<
11-18 03:31:22.819  6376  6376 F DEBUG   : uid: 10167
11-18 03:31:22.819  6376  6376 F DEBUG   : signal 6 (SIGABRT), code -1 (SI_QUEUE), fault addr --------
11-18 03:31:22.819  6376  6376 F DEBUG   : Abort message: 'terminating due to uncaught exception of type utils::PreconditionPanic: Precondition
11-18 03:31:22.819  6376  6376 F DEBUG   : in AssetLoader.cpp:88: buffer must be a direct ByteBuffer'
11-18 03:31:22.972  6376  6376 F DEBUG   : backtrace:
11-18 03:31:22.972  6376  6376 F DEBUG   :       #00 pc 00000000000a1234  /apex/com.android.runtime/lib64/bionic/libc.so (abort+164)
11-18 03:31:22.972  6376  6376 F DEBUG   :       #02 pc 0000000000301225  /data/app/base.apk!libfilament-jni.so (offset 0x4588000)
11-18 03:31:22.972  6376  6376 F DEBUG   :       #03 pc 0000000000300c76  /data/app/base.apk!libfilament-jni.so (offset 0x4588000)
"""

# One test counted as failed, with no message attached: how a process that died mid-test appears in
# JUnit XML. This is the shape run 44227951941 actually produced.
# An ANR trace in the shape Android writes to /data/anr/, plus the dropbox entry that carries the
# subject. Gradle reports only "keyDispatchingTimedOut"; the reason and the main thread's stack are here.
ANR_TRACE = """Subject: Input dispatching timed out (com.davemega.hub/com.davemega.hub.MainActivity, 6309)
Process: com.davemega.hub
----- pid 6309 at 2026-11-18 07:45:12 -----
Cmd line: com.davemega.hub
Build fingerprint: 'google/sdk_gphone_x86_64/generic_x86_64_arm64:11/RSR1.240422.006/12134477:userdebug'
ABI: 'x86_64'
Build type: optimized
DALVIK THREADS (24):
"main" prio=5 tid=1 Native
  | group="main" sCount=1 dsCount=0 flags=1 obj=0x7195e9f8 self=0x7f2c8c001000
  | sysTid=6309 nice=-10 cgrp=default sched=0/0 handle=0x7f2cdb0f04f8
  | state=S schedstat=( 44128871234 0 0 ) utm=3911 stm=501 core=1 HZ=100
  native: #00 pc 00000000000d1a3f  /apex/com.android.runtime/lib64/bionic/libc.so (syscall+31)
  at android.os.MessageQueue.nativePollOnce(Native method)
  at android.os.MessageQueue.next(MessageQueue.java:335)
  at android.os.Looper.loop(Looper.java:206)
  at android.app.ActivityThread.main(ActivityThread.java:7406)
  at java.lang.reflect.Method.invoke(Native method)
"AvatarForge-GLES" prio=5 tid=22 Runnable
  | sysTid=6402 nice=0 cgrp=default sched=0/0 handle=0x7f2b1c0f04f8
  | state=R schedstat=( 401223344000 0 0 ) utm=39880 stm=242 core=0 HZ=100
  at com.davemega.hub.avatarforge.AvatarForgeSurfaceView.nativeRender(Native method)
"""

GRADLE_ANR = """\
> Task :app:connectedDebugAndroidTest FAILED
com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest > generatedAvatarMustProduceVisiblePixels FAILED
    Test run failed to complete. Instrumentation run failed due to keyDispatchingTimedOut at TestRunner.java:445
BUILD FAILED in 12m 26s
"""

ANR_SUITE = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="com.davemega.hub.AvatarForgeRendererSmokeTest" tests="2" failures="1" errors="0" skipped="0">
  <testcase name="generatedAvatarMustProduceVisiblePixels" classname="com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest" time="241.0">
    <failure message="Instrumentation run failed due to keyDispatchingTimedOut" type="java.lang.AssertionError"/>
  </testcase>
</testsuite>
"""

SUITE_COUNTS_NO_DETAIL = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<testsuite name="com.davemega.hub.AvatarForgeRendererSmokeTest" tests="1" failures="1"'
    ' errors="0" skipped="0">\n'
    '  <testcase name="generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels"\n'
    '            classname="com.davemega.hub.AvatarForgeRendererSmokeTest" time="408.1"/>\n'
    '</testsuite>\n'
)


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
        # ...and the failing test still gets its own annotation with its own message. Filtered on the
        # title rather than the test name, because the totals annotation now names every test it found
        # and would otherwise be the first line to match.
        named = [ln for ln in lines if ln.startswith("::error title=failure:")]
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

    # ---- 15. another process's boot noise must not be reported as our crash ----
    # This is the exact failure of run 44227951941: five annotations about lowmemorykiller startup and
    # com.google.android.sdksetup, and nothing about the process that actually died.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        lines = commands_with_logcat(root, "Instrumentation run failed due to Process crashed.\n",
                                     BOOT_NOISE_LOGCAT)
        assert not any("lowmemorykiller" in ln for ln in lines), lines
        assert not any("sdksetup" in ln for ln in lines), lines
        assert not any("AppWidgetServiceImpl" in ln for ln in lines), lines
        assert reporter.collect_crash_blocks([str(_write(root, "lc.txt", BOOT_NOISE_LOGCAT))]) == []

    # ---- 16. a kill of OUR process must be reported, and named as a kill ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        lines = commands_with_logcat(root, "Instrumentation run failed due to Process crashed.\n",
                                     KILLED_LOGCAT)
        kills = [ln for ln in lines if "title=process-killed" in ln]
        assert kills, lines
        assert any("Kill 'com.davemega.hub'" in ln for ln in kills), kills
        assert any("Killing 2431:com.davemega.hub" in ln for ln in kills), kills
        # ...while the other package's kill is still excluded.
        assert not any("sdksetup" in ln for ln in kills), kills

    # ---- 17. the "counted but no message" signal must survive a full annotation budget ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, SUITE_COUNTS_NO_DETAIL)
        noisy = KILLED_LOGCAT + "".join(
            "11-18 03:12:%02d.000  999  999 F DEBUG   : filler frame %d at com.example.Noise\n" % (i, i)
            for i in range(40))
        lines = commands_with_logcat(root, "BUILD FAILED in 6m 48s\n", noisy)
        joined = "\n".join(lines)
        assert "carried a message" in joined, joined
        assert "tests=1 failures=1" in joined, joined

    # ---- 18. a wrapped abort message must arrive as ONE annotation, complete ----
    # The real crash read "utils::PreconditionPanic: Precondition" and stopped there, because the file,
    # line and condition that failed were on the next logcat line and the cap dropped it.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        path = _write(root, "logcat-crash.txt", TOMBSTONE_LOGCAT)
        blocks = reporter.collect_crash_blocks([str(path)])
        assert len(blocks) == 1, [b[1][0] for b in blocks]
        aborts = [ln for ln in blocks[0][1] if "Abort message" in ln]
        assert len(aborts) == 1, aborts
        assert "utils::PreconditionPanic" in aborts[0], aborts[0]
        # The half that was being lost:
        assert "AssetLoader.cpp:88" in aborts[0], aborts[0]
        assert "must be a direct ByteBuffer" in aborts[0], aborts[0]
        # Merging must not swallow the fields after the message.
        joined = "\n".join(blocks[0][1])
        assert "backtrace:" in joined, joined
        assert "libfilament-jni.so" in joined, joined
        assert "Build fingerprint" in joined, joined

        lines = commands_with_logcat(root, "Instrumentation run failed due to Process crashed.\n",
                                     TOMBSTONE_LOGCAT)
        errors = [ln for ln in lines if ln.startswith("::error")]
        assert any("AssetLoader.cpp:88" in ln for ln in errors), errors
        assert any("Fatal signal 6 (SIGABRT)" in ln for ln in errors), errors
        assert any("libfilament-jni.so" in ln for ln in errors), errors

    # ---- 19. merging must not run away when the closing quote never arrives ----
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        # A header is required to open a block at all, which is itself the point of narrowing the
        # header list: "Abort message:" no longer starts one.
        truncated = ("F libc    : Fatal signal 6 (SIGABRT), code -1 (SI_QUEUE) in tid 1 (a.b)\n"
                     "F DEBUG   : Abort message: 'terminating due to uncaught exception\n"
                     + "".join("F DEBUG   : continuation %d\n" % i for i in range(20)))
        path = _write(root, "lc.txt", truncated)
        blocks = reporter.collect_crash_blocks([str(path)])
        assert len(blocks) == 1, blocks
        aborts = [ln for ln in blocks[0][1] if "Abort message" in ln]
        assert len(aborts) == 1, aborts
        assert aborts[0].count("continuation") <= 5, aborts[0]

    # ---- 20. instrumentation teardown must not be reported as a kill on a green run ----
    # Android tears our process down at the end of every instrumentation run and that line names our
    # package. Two such reasons have now reached this code as FAILURE-level annotations on runs whose
    # tests had all passed: "due to finished inst" (run 44233306427) and "due to deletePackageX"
    # (run 44240305554) - the second getting straight through the filter added for the first. So the
    # level is decided by the totals rather than by recognising the reason, and this pins an
    # unrecognised reason as well, because the next teardown string Android invents will not be on any
    # list either.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, PASSING_SUITE)

        def kill_line(reason: str) -> str:
            return ("11-18 04:06:10.331   497   524 I ActivityManager: Killing "
                    "6110:com.davemega.hub/u0a167 (adj 900): stop com.davemega.hub due to "
                    + reason + "\n")

        # The invariant that holds for every reason, enumerated or not: never failure-level on a run
        # whose tests all passed. A recognised teardown reason is dropped outright; an unrecognised one
        # still surfaces, as a notice, because it is true - it is simply not the reason for anything.
        for reason in ("finished inst", "deletePackageX", "too many empty"):
            lines = commands_with_logcat(root, "BUILD SUCCESSFUL in 14m 2s\n", kill_line(reason))
            assert not any(ln.startswith("::error") for ln in lines), (reason, lines)
            assert not any("title=process-killed" in ln for ln in lines), (reason, lines)

        novel = kill_line("some-reason-nobody-enumerated")
        lines = commands_with_logcat(root, "BUILD SUCCESSFUL in 14m 2s\n", novel)
        assert not any(ln.startswith("::error") for ln in lines), lines
        assert not any("title=process-killed::" in ln for ln in lines), lines
        assert any("title=process-killed-after-success::" in ln for ln in lines), lines

        # A kill of our package on a run that actually failed is still a cause, at failure level.
        write(root, CONNECTED, FAILING_SUITE)
        lines = commands_with_logcat(root, "BUILD FAILED in 14m 2s\n", KILLED_LOGCAT)
        assert any(ln.startswith("::error") and "title=process-killed::" in ln for ln in lines), lines
        assert not any("title=process-killed-after-success::" in ln for ln in lines), lines

    # ---- 21. one suite element covering two classes must still name both ----
    # A connected run reported "tests=7 classes=...SmokeTest" for seven tests spread over two classes,
    # because the runner put them all under one suite name. Under-reporting which classes ran is the
    # exact failure this tool exists to prevent.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        combined = (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<testsuite name="com.davemega.hub.AvatarForgeRendererSmokeTest" tests="7" failures="0"'
            ' errors="0" skipped="0">\n'
            + "".join(
                '  <testcase name="smoke%d" classname="com.davemega.hub.AvatarForgeRendererSmokeTest"'
                ' time="1.0"/>\n' % i for i in range(3))
            + "".join(
                '  <testcase name="runtime%d"'
                ' classname="com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest" time="1.0"/>\n'
                % i for i in range(4))
            + "</testsuite>\n")
        write(root, CONNECTED, combined)
        tests, failed, errors, skipped, classes = reporter.collect_test_totals(str(root))
        assert (tests, failed, errors, skipped) == (7, 0, 0, 0), (tests, failed, errors, skipped)
        assert classes == ["com.davemega.hub.AvatarForgeRendererSmokeTest",
                           "com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest"], classes

    # ---- 22. totals must name the tests, so stale XML from another step is visible ----
    # avatarforge-diagnostic-apk.yml runs the instrumentation task twice in one job and both steps write
    # to the same results tree. A step that dies before writing XML leaves the previous step's passing
    # XML behind, and tests=1 failures=0 then describes the wrong step. Counts cannot show that; names
    # can. The script now deletes the tree before each invocation, and this pins the reporting half.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        step_a = ('<?xml version="1.0" encoding="UTF-8"?>\n'
                  '<testsuite name="com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest" tests="1"'
                  ' failures="0" errors="0" skipped="0">\n'
                  '  <testcase name="generatedAvatarMustProduceVisiblePixels"'
                  ' classname="com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest" time="1.0"/>\n'
                  '</testsuite>\n')
        write(root, CONNECTED, step_a)
        lines = commands(root, "BUILD SUCCESSFUL in 3m 16s\n")
        assert any("tests=[GeneratedAvatarRuntimeTest#generatedAvatarMustProduceVisiblePixels]" in ln
                   for ln in lines), lines
        # A second step's XML next to the first shows both, which is how a leftover is recognised.
        write(root, "app/build/outputs/androidTest-results/connected/pixel_2/TEST-second.xml",
              step_a.replace("generatedAvatarMustProduceVisiblePixels",
                             "generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView"))
        lines = commands(root, "BUILD FAILED in 3m 16s\n")
        joined = "\n".join(lines)
        assert "generatedAvatarMustProduceVisiblePixels" in joined, lines
        assert "generatedAvatarExportedGlbMustRenderThroughProductionSurfaceView" in joined, lines
        assert "tests=2" in joined, lines
        counts = subprocess.run(
            [sys.executable, str(MODULE_PATH), str(root / "gradle.log"), "--root", str(root),
             "--counts"], capture_output=True, text=True)
        assert "tests=[" in counts.stdout, counts.stdout

    # ---- 23. an ANR trace must surface its reason and the main thread, not just Gradle's symptom ----
    # Run 44259843505 aborted with "Instrumentation run failed due to keyDispatchingTimedOut" after two
    # of four tests. That names a symptom. The subject line says what timed out and the trace says what
    # the main thread and the render thread were doing, which is the difference between knowing an ANR
    # happened and knowing whether the app blocked itself or was starved of cores.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        write(root, CONNECTED, ANR_SUITE)
        lines = commands_with_logcat(root, GRADLE_ANR, ANR_TRACE)
        joined = "\n".join(lines)
        assert "keyDispatchingTimedOut" in joined, lines
        assert "Input dispatching timed out" in joined, lines
        assert 'Cmd line: com.davemega.hub' in joined, lines
        # The main thread's stack has to survive: DALVIK THREADS must not terminate the block early.
        assert '\"main\" prio=5 tid=1' in joined, lines
        assert "nativePollOnce" in joined, lines
        # And the render thread that was competing with it for the two cores.
        assert "AvatarForge-GLES" in joined or "nativeRender" in joined, lines

    print("PASS: connected + JVM discovery, failure detail, totals, the 0/1/2 gate statuses, "
          "always-annotated totals, crash detection, Gradle-frame deprioritisation, logcat crash "
          "blocks, block integrity, cause-over-context ranking, per-level budgets, boot-noise "
          "rejection, package-aware kill detection, mismatch survival under a full budget, "
          "wrapped abort-message merging, kill level decided by totals rather than by enumerating "
          "teardown reasons, class names taken from testcases when one suite element covers several, "
          "totals naming every test they found, and ANR traces surfacing subject plus main thread")


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
# Instrumented AvatarForge GLES render smoke.
#
# WHY THIS SCRIPT LOOKS LIKE THIS
# -------------------------------
# This job failed for weeks with the only reachable signal being "The process '/usr/bin/sh' failed with
# exit code 1". The evidence it did collect (logcat, diagnostics JSON, PNGs) is uploaded as an artifact,
# but artifacts are served from a blob host that a sandbox cannot reach, and job logs are served from a
# results host that is likewise unreachable. So the failure was real, and undiagnosable from anywhere
# except a browser.
#
# Everything below therefore exists to move the diagnosis into the two channels that ARE reachable
# programmatically: check-run ANNOTATIONS and the JOB SUMMARY. tools/ci_report_test_failures.py reads the
# JUnit XML and the tee'd Gradle log on the runner and re-emits the parts that matter, including the
# failing test's own assertion message and the first stack frame in our code.
#
# The pass/fail gate is derived from that XML, not from grepping logcat for a hard-coded test count. A
# literal "run finished: 3 tests" matched a suite that had silently stopped running one of its tests, and
# a gate that hard-codes a count cannot tell "all three passed" from "one never ran".
set -uo pipefail

EVIDENCE="avatarforge-render-smoke-evidence"
GRADLE_LOG="$EVIDENCE/gradle-connected.log"
mkdir -p "$EVIDENCE"
chmod +x ./gradlew

# The classes this job is responsible for. GeneratedAvatarRuntimeTest is deliberately NOT here yet: its
# four tests carry timeouts of 900 s + 600 s + 600 s + 600 s, which on a -gpu swangle software
# rasterizer can exceed this job's whole budget on their own and would turn a diagnosable failure into a
# timeout that reports nothing. It is the next thing to add once this suite is green and its real runtime
# is known. Recorded here so the omission is a decision on the record, not an oversight.
TEST_CLASSES="com.davemega.hub.AvatarForgeRendererSmokeTest"

TEST_STATUS=0
./gradlew --no-daemon :app:connectedDebugAndroidTest \
  "-Pandroid.testInstrumentationRunnerArguments.class=$TEST_CLASSES" \
  --stacktrace 2>&1 | tee "$GRADLE_LOG" || TEST_STATUS=$?

# connectedDebugAndroidTest tears down the instrumented target/test packages after the suite. The app
# process and app-private files are therefore not valid postconditions here. The renderer tests assert
# PixelCopy success and screenshot creation while the app is installed; the app also exports durable
# evidence to shared Downloads before instrumentation cleanup.
adb logcat -d -v threadtime > "$EVIDENCE/logcat.txt" 2>&1 || true
adb logcat -d -v threadtime -s AvatarForge:V > "$EVIDENCE/avatarforge-logcat.txt" 2>&1 || true
adb shell dumpsys activity activities > "$EVIDENCE/activities.txt" 2>&1 || true
adb exec-out screencap -p > "$EVIDENCE/final-screen.png" 2>/dev/null || true

DIAGNOSTICS_DIR="/sdcard/Download/3D Mega Hub Diagnostics"
adb pull "$DIAGNOSTICS_DIR/renderer_diagnostics.json" "$EVIDENCE/renderer_diagnostics.json" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/imported_glb_diagnostic.png" "$EVIDENCE/imported_glb_diagnostic.png" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/generated_avatar_diagnostic.png" "$EVIDENCE/generated_avatar_diagnostic.png" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/failed_render.png" "$EVIDENCE/failed_render.png" >/dev/null 2>&1 || true

# Report BEFORE any gate can exit, so a gate failure never costs us the diagnosis.
python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . >> "$GITHUB_STEP_SUMMARY" 2>/dev/null || true
python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . --commands || true

COUNTS="$(python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . --counts 2>/dev/null)"
COUNTS_STATUS=$?
echo "instrumented results: ${COUNTS:-<none>}"
{
  echo "### Instrumented renderer results"
  echo
  echo '```'
  echo "classes requested: $TEST_CLASSES"
  echo "${COUNTS:-no JUnit XML was produced}"
  echo "gradle exit: $TEST_STATUS"
  echo '```'
} >> "$GITHUB_STEP_SUMMARY" 2>/dev/null || true

if [[ "$TEST_STATUS" -ne 0 ]]; then
  echo "FAIL: instrumented AvatarForge GLES GLB renderer test failed with exit $TEST_STATUS."
  echo "::error title=instrumented-renderer::connectedDebugAndroidTest exited $TEST_STATUS; see the annotated test failures above"
  exit "$TEST_STATUS"
fi

# counts exit status: 0 = ran and all passed, 1 = something failed, 2 = no XML at all.
if [[ "$COUNTS_STATUS" -eq 2 ]]; then
  echo "FAIL: no instrumented JUnit XML was produced, so nothing proves any test ran."
  echo "::error title=instrumented-renderer::no JUnit XML found - the tests never ran or the results path changed"
  exit 1
fi
if [[ "$COUNTS_STATUS" -ne 0 ]]; then
  echo "FAIL: instrumented tests ran but reported failures."
  exit 1
fi

# Presentation evidence. These are the claims the tests themselves cannot make after teardown, so they
# are checked from what the app exported while it was still installed.
if ! grep -q '"status"[[:space:]]*:[[:space:]]*"PIXELS_VERIFIED"' "$EVIDENCE/renderer_diagnostics.json" 2>/dev/null; then
  echo "FAIL: persisted renderer JSON does not prove PIXELS_VERIFIED."
  echo "::error title=instrumented-renderer::renderer_diagnostics.json is missing or does not say PIXELS_VERIFIED"
  exit 1
fi
for marker in "GLB accessor semantic=POSITION" "GLB packed vertex pool" "af_vertex layout" "GPU Vertex 0" "MVP clip vertex 0" "DrawElements submesh="; do
  if ! grep -Fq "$marker" "$EVIDENCE/avatarforge-logcat.txt" 2>/dev/null; then
    echo "FAIL: AvatarForge logcat evidence is missing diagnostic marker: $marker"
    echo "::error title=instrumented-renderer::AvatarForge logcat is missing the marker '$marker', so the GLES path is not proven end to end"
    exit 1
  fi
done
if [[ ! -s "$EVIDENCE/imported_glb_diagnostic.png" ]]; then
  echo "FAIL: durable imported-GLB PixelCopy screenshot is missing."
  echo "::error title=instrumented-renderer::imported_glb_diagnostic.png was not exported by the app"
  exit 1
fi
if [[ -s "$EVIDENCE/failed_render.png" ]]; then
  echo "FAIL: renderer exported failed_render.png; inspect the captured failure."
  echo "::error title=instrumented-renderer::the renderer itself exported failed_render.png - a capture of the failing frame"
  exit 1
fi
echo "PASS: instrumented renderer tests passed; imported GLB PixelCopy JSON and PNG evidence captured."
echo "instrumented results: $COUNTS"

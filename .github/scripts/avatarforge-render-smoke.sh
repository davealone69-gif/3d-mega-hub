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

# Reused by three workflows, so what it runs and where it writes are configurable:
#   AF_TEST_TARGET            class list or Class#method for -Pandroid.testInstrumentationRunnerArguments.class
#   AF_EVIDENCE_DIR           where the logcat/JUnit/diagnostic evidence is collected
#   AF_REQUIRE_SMOKE_EVIDENCE 1 gates on AvatarForgeRendererSmokeTest's own exports; 0 skips those gates
# The reporting and the XML-derived gate are identical everywhere, because the whole reason this script
# exists is that three separate jobs all failed for weeks with the only reachable signal being
# "The process '/usr/bin/sh' failed with exit code 1". avatarforge-diagnostic-apk.yml and
# avatarforge-generated-runtime.yml were still in that state: both ran the production SurfaceView test,
# both failed on head 6a32184 (steps of 8m43s and 13m08s, so neither was a job timeout), and neither
# emitted a single test annotation. Reusing this script rather than copying it means a fix to the
# reporter reaches all three jobs at once.
AF_TEST_TARGET="${AF_TEST_TARGET:-com.davemega.hub.AvatarForgeRendererSmokeTest,com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest}"
AF_REQUIRE_SMOKE_EVIDENCE="${AF_REQUIRE_SMOKE_EVIDENCE:-1}"
EVIDENCE="${AF_EVIDENCE_DIR:-avatarforge-render-smoke-evidence}"
GRADLE_LOG="$EVIDENCE/gradle-connected.log"
mkdir -p "$EVIDENCE"
chmod +x ./gradlew

# The classes this job is responsible for.
#
# GeneratedAvatarRuntimeTest was held back while the smoke suite was red: its four tests carry timeouts
# of 900 s + 600 s + 600 s + 600 s, which on a -gpu swangle software rasterizer could exceed this job's
# whole budget on their own, turning a diagnosable failure into a timeout that reports nothing. The
# condition for adding it was the smoke suite going green and its real runtime being known, and both are
# now true - run 44233306427 reported tests=3 failures=0 and the job took 14m51s end to end, emulator
# boot and Gradle build included.
#
# That is the production route and it is what this job exists to prove: it builds an
# AvatarForgeSurfaceView on the generated GLB directly rather than going through the studio screen, so
# it covers generated-GLB-to-GLES-SurfaceView independent of any UI routing. Leaving it out let a green
# job stand for less than it appeared to. The worst case is now 15 min + 45 min of timeouts, so the
# job's timeout-minutes is raised to 75 to match; a job that times out reports nothing at all, which is
# the one outcome worse than a slow one.
TEST_CLASSES="$AF_TEST_TARGET"

# Delete the previous invocation's results before running. avatarforge-diagnostic-apk.yml calls this
# script twice in one job, from two emulator steps, and both write to the same
# app/build/outputs/androidTest-results tree. A step whose instrumentation dies before writing XML
# leaves the earlier step's passing XML in place, and the report then reads tests=1 failures=0 for a
# step that had just failed - a pass invented out of somebody else's evidence. Removing the tree makes
# "no XML" mean "this invocation produced none", which the gate below already treats as a failure.
rm -rf app/build/outputs/androidTest-results app/build/reports/androidTests 2>/dev/null || true

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

# "Instrumentation run failed due to Process crashed" is all Gradle ever says, and the main logcat
# buffer is full of other processes' boot noise and rolls over. Android keeps a dedicated crash buffer
# and dropbox retains crash/ANR/native-crash entries after the process is gone; a native crash also
# leaves a tombstone carrying the backtrace that names the faulting library. Those three are where the
# reason actually lives, and collecting only logcat.txt is why a crash stayed undiagnosed.
adb logcat -b crash -d -v threadtime > "$EVIDENCE/logcat-crash.txt" 2>&1 || true
adb shell dumpsys dropbox --print data_app_crash data_app_native_crash data_app_anr \
  system_app_crash system_app_native_crash 2>&1 | head -n 4000 > "$EVIDENCE/dropbox-crashes.txt" || true
# adb root is available on an emulator system image and is what makes /data/tombstones readable.
adb root >/dev/null 2>&1 || true
adb wait-for-device >/dev/null 2>&1 || true
adb shell ls -1 /data/tombstones/ > "$EVIDENCE/tombstones-list.txt" 2>&1 || true
mkdir -p "$EVIDENCE/tombstones"
adb pull /data/tombstones/ "$EVIDENCE/tombstones/" >/dev/null 2>&1 || true
# ANR traces. An ANR ends an instrumentation run the same way a crash does, and all Gradle ever says is
# "Instrumentation run failed due to keyDispatchingTimedOut" - which names the symptom and not the cause.
# Run 44259843505 died exactly that way with two of four tests executed, and the report could add nothing
# to the Gradle line because nothing collected /data/anr/. The trace there holds the main thread's stack
# at the moment of the ANR, which is the difference between "the app stopped answering input" and knowing
# whether it was blocked on a lock, blocked in our own code, or simply starved of cores by a software
# rasterizer. Tombstones were already pulled for native crashes; this is the ANR equivalent.
adb shell ls -1 /data/anr/ > "$EVIDENCE/anr-list.txt" 2>&1 || true
mkdir -p "$EVIDENCE/anr"
adb pull /data/anr/ "$EVIDENCE/anr/" >/dev/null 2>&1 || true
# Why each of our processes ended, as Android itself records it: REASON_ANR, REASON_CRASH,
# REASON_SIGNALED, REASON_LOW_MEMORY, each with its own description and timestamp. The test package is
# asked for as well as the app, since instrumentation runs in either.
{
  adb shell dumpsys activity exit-info com.davemega.hub 2>&1
  adb shell dumpsys activity exit-info com.davemega.hub.test 2>&1
} | head -n 2000 > "$EVIDENCE/process-exit-info.txt" 2>&1 || true
adb shell dumpsys activity activities > "$EVIDENCE/activities.txt" 2>&1 || true
adb exec-out screencap -p > "$EVIDENCE/final-screen.png" 2>/dev/null || true

# Device-side captures that used to live in avatarforge-generated-runtime.yml's own script block, moved
# here so that this script's exit status IS the step's exit status. That block captured the status with
# `test_status=$?` and ended with `exit "$test_status"`; across the emulator action's shell boundary the
# variable arrived EMPTY, and in POSIX sh `exit ""` is an illegal number, so the step exited 2 - which is
# how run 44276642189 failed with tests=4 failures=0 and a notice reading
# `shared_script_exit= block_reached_exit=1`. Every test had passed. Nothing here may change the exit
# status, so all of it ends in `|| true`.
adb exec-out screencap -p > "$EVIDENCE/emulator-screen.png" 2>/dev/null || true
adb shell pm list packages 2>/dev/null | grep -F com.davemega.hub > "$EVIDENCE/installed-packages.txt" || true
# The instrumentation test's own avatar PNGs and JSON live in app-internal storage; run-as reaches them
# on a debuggable build and avoids Android 11+ scoped-storage restrictions on /Android/data.
adb exec-out sh -c 'run-as com.davemega.hub sh -c "cd files/avatarforge-diagnostics && tar -cf - ."'   > "$EVIDENCE/app-diagnostics.tar" 2> "$EVIDENCE/run-as-error.txt" || true
[[ -s "$EVIDENCE/app-diagnostics.tar" ]] &&   tar -xf "$EVIDENCE/app-diagnostics.tar" -C "$EVIDENCE/" 2>/dev/null || true
{
  echo "build_commit=${GITHUB_SHA:-unknown}"
  echo "capture_time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "test_target=${TEST_CLASSES}"
  find "$EVIDENCE" -type f ! -name evidence-manifest.txt -print0 2>/dev/null | sort -z | xargs -0 -r sha256sum
} > "$EVIDENCE/evidence-manifest.txt" 2>/dev/null || true

DIAGNOSTICS_DIR="/sdcard/Download/3D Mega Hub Diagnostics"
adb pull "$DIAGNOSTICS_DIR/renderer_diagnostics.json" "$EVIDENCE/renderer_diagnostics.json" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/imported_glb_diagnostic.png" "$EVIDENCE/imported_glb_diagnostic.png" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/generated_avatar_diagnostic.png" "$EVIDENCE/generated_avatar_diagnostic.png" >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/failed_render.png" "$EVIDENCE/failed_render.png" >/dev/null 2>&1 || true

# Report BEFORE any gate can exit, so a gate failure never costs us the diagnosis.
# logcat is passed as well: Gradle only ever says "Instrumentation run failed due to Process crashed",
# while the reason - a Java FATAL EXCEPTION stack or a native Fatal signal backtrace - is in logcat.
# Ordered most specific first: the crash buffer and dropbox hold only crashes, whereas the main
# logcat holds the whole device's boot as well.
CRASH_LOGS=(
  --crash-log "$EVIDENCE/logcat-crash.txt"
  --crash-log "$EVIDENCE/dropbox-crashes.txt"
  --crash-log "$EVIDENCE/avatarforge-logcat.txt"
  --crash-log "$EVIDENCE/logcat.txt"
)
# Most specific first: an ANR trace and an exit-reason record are about one process and one moment,
# while the main logcat is the whole device's history.
for anr_trace in "$EVIDENCE"/anr/*; do
  [[ -f "$anr_trace" ]] && CRASH_LOGS+=(--crash-log "$anr_trace")
done
[[ -s "$EVIDENCE/process-exit-info.txt" ]] && CRASH_LOGS+=(--crash-log "$EVIDENCE/process-exit-info.txt")
for tombstone in "$EVIDENCE"/tombstones/*; do
  [[ -f "$tombstone" ]] && CRASH_LOGS+=(--crash-log "$tombstone")
done
python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . "${CRASH_LOGS[@]}" >> "$GITHUB_STEP_SUMMARY" 2>/dev/null || true
python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . "${CRASH_LOGS[@]}" --commands || true

# Tells the workflow's own if:always() reporting step that this already happened. Without it both
# report the same failure, and GitHub caps annotations at 10 per level per run - the duplicate halves
# how much of the diagnosis survives.
touch "$EVIDENCE/.reported" 2>/dev/null || true

COUNTS="$(python3 tools/ci_report_test_failures.py "$GRADLE_LOG" --root . --counts 2>/dev/null)"
COUNTS_STATUS=$?
# The job's own stdout is not readable programmatically - logs are served from a results host and the
# step summary is not exposed through the check-run API - so the resolved configuration and both exit
# codes are emitted as an annotation. Run 44262469686 exited 2 with all seven tests passing and nothing
# reachable said which gate had fired or even which tests had been requested; this line is what makes
# that question answerable in one read instead of by inference.
echo "::notice title=instrumented-config::target=${TEST_CLASSES} evidence=${EVIDENCE} smoke_gates=${AF_REQUIRE_SMOKE_EVIDENCE} gradle_exit=${TEST_STATUS} counts_exit=${COUNTS_STATUS} ${COUNTS:-no-junit-xml}"
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
#
# Gated on AF_REQUIRE_SMOKE_EVIDENCE because they are specific to AvatarForgeRendererSmokeTest: it is
# what exports imported_glb_diagnostic.png, what reaches PIXELS_VERIFIED, and what emits those logcat
# markers. A job running only GeneratedAvatarRuntimeTest would fail all four of these checks while its
# own test passed, which would be this script inventing a failure. The evidence is still COLLECTED and
# uploaded either way; only the gates are conditional.
if [[ "$AF_REQUIRE_SMOKE_EVIDENCE" != "1" ]]; then
  echo "::notice title=instrumented-gates::smoke-suite presentation gates skipped by request; the XML-derived gate above is the only pass condition for this job"
  echo "PASS: instrumented tests passed. Smoke-suite presentation gates skipped (AF_REQUIRE_SMOKE_EVIDENCE=$AF_REQUIRE_SMOKE_EVIDENCE); evidence was still collected in $EVIDENCE/."
  echo "instrumented results: $COUNTS"
  exit 0
fi
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

#!/usr/bin/env bash
# Runs the generated-avatar instrumentation tests on the already-booted emulator and ALWAYS
# collects the evidence, whether the tests pass or fail.
#
# android-emulator-runner executes every line of an inline `script:` in its own shell and stops at
# the first failing line, so the previous inline version (`set +e`, run Gradle, save `$?`, collect,
# `exit $status`) never reached its collection commands when a test failed: a red run uploaded no
# renderer evidence at all. Keeping the whole sequence in one script fixes that.
#
# Usage: generated-runtime.sh [instrumentation class or class#method]
set -uo pipefail

TEST_FILTER="${1:-com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest}"
OUT="artifacts/avatarforge/runtime-captures"
mkdir -p "$OUT"
chmod +x ./gradlew

adb logcat -c || true
adb logcat -G 16M || true

TEST_STATUS=0
./gradlew --no-daemon --stacktrace :app:connectedDebugAndroidTest \
  "-Pandroid.testInstrumentationRunnerArguments.class=${TEST_FILTER}" || TEST_STATUS=$?

# connectedDebugAndroidTest uninstalls the app when it finishes, so app-private files are gone by
# now. Evidence therefore comes from logcat and from the public Downloads folder, both of which
# survive the uninstall.
adb logcat -d -v threadtime > "$OUT/logcat.txt" 2>&1 || true
adb logcat -d -v threadtime -s AvatarForge:V > "$OUT/avatarforge-logcat.txt" 2>&1 || true
adb logcat -d -b crash -v threadtime > "$OUT/crash-logcat.txt" 2>&1 || true
adb exec-out screencap -p > "$OUT/emulator-screen.png" 2>/dev/null || true

DIAGNOSTICS_DIR="/sdcard/Download/3D Mega Hub Diagnostics"
for name in renderer_diagnostics.json renderer_telemetry.json generated_avatar_diagnostic.png \
            imported_glb_diagnostic.png failed_render.png; do
  adb pull "$DIAGNOSTICS_DIR/$name" "$OUT/$name" >/dev/null 2>&1 || true
done

{
  echo "build_commit=${GITHUB_SHA:-unknown}"
  echo "test_filter=${TEST_FILTER}"
  echo "gradle_exit_status=${TEST_STATUS}"
  echo "capture_time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "emulator_sdk=$(adb shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r')"
  echo "emulator_abi=$(adb shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r')"
  find "$OUT" -type f ! -name evidence-manifest.txt -print0 | sort -z | xargs -0 -r sha256sum
} > "$OUT/evidence-manifest.txt"

echo "::group::Evidence manifest"
cat "$OUT/evidence-manifest.txt"
echo "::endgroup::"

# Put the measured renderer evidence in the job log itself so a result can be read without
# downloading the artifact.
echo "::group::Measured renderer evidence (logcat, tag AvatarForge)"
grep -E "TELEMETRY |EVIDENCE|SLOW_START|STALL |FRAME_PRESENTED|Renderer failed|Avatar LOD|GPU upload" \
  "$OUT/avatarforge-logcat.txt" | sed -E 's/^[0-9-]+ +[0-9:.]+ +[0-9]+ +[0-9]+ +//' | tail -n 60 || true
echo "::endgroup::"
for name in renderer_diagnostics.json renderer_telemetry.json; do
  if [[ -s "$OUT/$name" ]]; then
    echo "::group::$name"
    cat "$OUT/$name"
    echo
    echo "::endgroup::"
  fi
done
echo "::group::Crashes, ANRs and fatal signals seen by the emulator"
grep -E "FATAL EXCEPTION|Fatal signal|ANR in com\.davemega\.hub|Process: com\.davemega\.hub" "$OUT/logcat.txt" | tail -n 20 \
  || echo "none found in logcat"
echo "::endgroup::"

if [[ "$TEST_STATUS" -ne 0 ]]; then
  echo "FAIL: instrumentation exited with status $TEST_STATUS for ${TEST_FILTER}. Evidence was collected above."
  exit "$TEST_STATUS"
fi
echo "PASS: ${TEST_FILTER} passed; evidence collected."

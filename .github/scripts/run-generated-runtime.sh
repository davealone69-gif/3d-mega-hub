#!/usr/bin/env bash
# Runs GeneratedAvatarRuntimeTest on the Android emulator, always captures evidence
# (screencap, logcat, app diagnostics, sha256 manifest), and exits with Gradle's exact code.
set +e
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest \
  --stacktrace
test_status=$?

OUT="artifacts/avatarforge/runtime-captures"
mkdir -p "$OUT"

adb exec-out screencap -p > "$OUT/emulator-screen.png" || true
adb logcat -d > "$OUT/logcat.txt" || true
adb logcat -d -s AvatarForge:V GeneratedAvatarRuntimeTest:V > "$OUT/avatarforge-logcat.txt" || true
adb shell pm list packages | grep -F com.davemega.hub > "$OUT/installed-packages.txt" || true
adb exec-out sh -c 'run-as com.davemega.hub sh -c "cd files/avatarforge-diagnostics && tar -cf - ."' \
  > "$OUT/avatarforge-diagnostics.tar" 2> "$OUT/run-as-error.txt" || true
if [ -s "$OUT/avatarforge-diagnostics.tar" ]; then
  tar -xf "$OUT/avatarforge-diagnostics.tar" -C "$OUT/" 2>> "$OUT/run-as-error.txt" || true
fi

find "$OUT" -type f -printf '%p %s bytes\n' || true
echo "build_commit=${GITHUB_SHA:-unknown}" > "$OUT/evidence-manifest.txt"
echo "capture_time_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$OUT/evidence-manifest.txt"
echo "gradle_exit_code=$test_status" >> "$OUT/evidence-manifest.txt"
find "$OUT" -type f ! -name evidence-manifest.txt -print0 | sort -z | xargs -0 -r sha256sum >> "$OUT/evidence-manifest.txt" || true
cat "$OUT/evidence-manifest.txt" || true

exit "$test_status"

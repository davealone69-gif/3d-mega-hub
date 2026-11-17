#!/usr/bin/env bash
# Runs on the CI host with adb available, immediately after the emulator has booted.
#
# Everything it learns is written to runtime-evidence/summary.md, which the workflow posts as a pull
# request comment. That indirection exists because this repository's workflow logs and artifacts are
# served from hosts that are not always reachable, while comments are.
#
# No step is allowed to abort the capture: a missing screenshot is still evidence.
set -u

APP_ID=com.davemega.hub
RUNTIME_CLASS=com.davemega.hub.avatarforge.GeneratedAvatarRuntimeTest
SMOKE_CLASS=com.davemega.hub.AvatarForgeRendererSmokeTest
OUT=runtime-evidence
SUMMARY="$OUT/summary.md"

mkdir -p "$OUT"
: > "$SUMMARY"

log() { echo "$*" >> "$SUMMARY"; }
section() { log ""; log "### $*"; }
fence() { log '```text'; "$@" >> "$SUMMARY" 2>&1; log '```'; }

section "Device"
{
  echo '```text'
  adb devices
  echo "sdk=$(adb shell getprop ro.build.version.sdk | tr -d '\r') abi=$(adb shell getprop ro.product.cpu.abi | tr -d '\r')"
  echo "model=$(adb shell getprop ro.product.model | tr -d '\r')"
  echo "gles=$(adb shell dumpsys SurfaceFlinger 2>/dev/null | grep -m1 -i 'GLES' | tr -d '\r')"
  echo '```'
} >> "$SUMMARY" 2>&1

section "Install"
adb install -r -t app/build/outputs/apk/debug/app-debug.apk > "$OUT/install-app.log" 2>&1
APP_INSTALL=$?
adb install -r -t app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk > "$OUT/install-test.log" 2>&1
TEST_INSTALL=$?
log "- debug APK install exit: \`$APP_INSTALL\`"
log "- androidTest APK install exit: \`$TEST_INSTALL\`"
if [ "$APP_INSTALL" -ne 0 ]; then
  { echo '```text'; tail -20 "$OUT/install-app.log"; echo '```'; } >> "$SUMMARY"
fi

if [ "$APP_INSTALL" -eq 0 ]; then
  APK_SIZE=$(stat -c%s app/build/outputs/apk/debug/app-debug.apk 2>/dev/null || echo unknown)
  APK_SHA=$(sha256sum app/build/outputs/apk/debug/app-debug.apk 2>/dev/null | cut -d' ' -f1)
  log "- installed APK: \`$APK_SIZE\` bytes, sha256 \`${APK_SHA:-unknown}\`"
fi

section "Level 3/4: launch and look at the screen"
adb logcat -c > /dev/null 2>&1
adb shell am force-stop "$APP_ID" > /dev/null 2>&1
adb shell am start -W -n "$APP_ID/.MainActivity" > "$OUT/launch.log" 2>&1
log "- \`am start -W\` exit: \`$?\`"
{ echo '```text'; head -12 "$OUT/launch.log"; echo '```'; } >> "$SUMMARY"

sleep 12

adb shell dumpsys activity activities 2>/dev/null \
  | grep -E "mResumedActivity|ResumedActivity|topResumedActivity" | head -3 > "$OUT/resumed.txt"
{ echo '```text'; cat "$OUT/resumed.txt"; echo '```'; } >> "$SUMMARY"

capture_frame() {
  local name="$1"
  adb exec-out screencap > "$OUT/$name.raw" 2> "$OUT/$name.err"
  python3 tools/ci/analyze_raw_screencap.py "$OUT/$name.raw" > "$OUT/$name.txt" 2>&1
  { echo '```text'; cat "$OUT/$name.txt" 2>/dev/null || echo "no pixel analysis"; echo '```'; } >> "$SUMMARY"
}

capture_frame frame-launch

section "Interaction stress (monkey, fixed seed)"
adb shell monkey -p "$APP_ID" -s 20261116 --throttle 60 --pct-syskeys 0 -v 150 > "$OUT/monkey.log" 2>&1
MONKEY=$?
log "- \`monkey\` exit: \`$MONKEY\` (0 means no crash or ANR)"
{ echo '```text'; grep -E "Events injected|// (CRASH|NOT RESPONDING)|Monkey finished" "$OUT/monkey.log" | head -6; echo '```'; } >> "$SUMMARY"

sleep 3
capture_frame frame-after-monkey

adb shell dumpsys activity activities 2>/dev/null \
  | grep -E "mResumedActivity" | head -2 > "$OUT/resumed-after.txt"
{ echo '```text'; cat "$OUT/resumed-after.txt"; echo '```'; } >> "$SUMMARY"

section "Crashes"
adb logcat -d -b crash > "$OUT/logcat-crash.txt" 2>&1
CRASH_LINES=$(wc -l < "$OUT/logcat-crash.txt" | tr -d ' ')
log "- crash buffer lines: \`$CRASH_LINES\`"
if [ "$CRASH_LINES" -gt 0 ]; then
  { echo '```text'; head -30 "$OUT/logcat-crash.txt"; echo '```'; } >> "$SUMMARY"
fi

adb logcat -d 2>/dev/null \
  | grep -iE "filament|avatarforge|AndroidRuntime|FATAL|UnsatisfiedLink|av_" \
  | head -60 > "$OUT/logcat-app.txt"
if [ -s "$OUT/logcat-app.txt" ]; then
  { echo '```text'; head -25 "$OUT/logcat-app.txt"; echo '```'; } >> "$SUMMARY"
fi

section "Memory"
{
  echo '```text'
  adb shell dumpsys meminfo "$APP_ID" 2>/dev/null | grep -E "TOTAL PSS|Java Heap|Native Heap|Graphics" | head -5
  echo '```'
} >> "$SUMMARY" 2>&1

section "App-written files"
{
  echo '```text'
  adb shell run-as "$APP_ID" ls -R files 2>/dev/null | head -15
  echo "--- external ---"
  adb shell ls -R "/sdcard/Android/data/$APP_ID/files" 2>/dev/null | head -15
  echo '```'
} >> "$SUMMARY" 2>&1

digest_instrumentation() {
  local class="$1"
  local log_file="$OUT/instrumentation-$(basename "$class").log"
  adb shell am instrument -w -r -e class "$class" \
    "$APP_ID.test/androidx.test.runner.AndroidJUnitRunner" > "$log_file" 2>&1
  log "- \`$class\` exit: \`$?\`"

  # Only the lines that carry a verdict; the full log stays in the artifacts.
  {
    clean="$OUT/instrumentation-$(basename "$class").clean.txt"
    tr -d '\r' < "$log_file" > "$clean"
    echo '```text'
    # One line per test verdict: START (code 1) and END (code 0/-1/-2) both appear.
    grep -E "^INSTRUMENTATION_STATUS: (test=|stack=|class=)" "$clean" \
      | sed 's/^INSTRUMENTATION_STATUS: //' | head -14
    grep -E "^INSTRUMENTATION_STATUS_CODE: -?[0-9]+" "$clean" | sed 's/^/code /' | head -12
    grep -E "^OK \(|^FAILURES|^Tests run:|^Time: |^INSTRUMENTATION_CODE|INSTRUMENTATION_RESULT" "$clean" | head -6
    if grep -q "stack=" "$clean"; then
      echo '--- first stack trace ---'
      grep -m1 -A6 "stack=" "$clean" | head -8
    fi
    echo '```'
  } >> "$SUMMARY"
}

section "Instrumentation: $RUNTIME_CLASS"
digest_instrumentation "$RUNTIME_CLASS"

section "Instrumentation: $SMOKE_CLASS"
digest_instrumentation "$SMOKE_CLASS"

log ""
log "_Generated by tools/ci/emulator_runtime_capture.sh on run ${GITHUB_RUN_ID:-local}._"
exit 0

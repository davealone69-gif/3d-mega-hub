#!/usr/bin/env bash
set -euo pipefail

mkdir -p emulator-diagnostics
APK="app/build/outputs/apk/debug/app-debug.apk"
test -s "$APK"

adb wait-for-device
BOOTED=""
for attempt in $(seq 1 90); do
  BOOTED="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)"
  if [[ "$BOOTED" == "1" ]]; then
    break
  fi
  sleep 2
done
if [[ "$BOOTED" != "1" ]]; then
  echo "Emulator did not reach sys.boot_completed=1 within 180 seconds."
  adb logcat -d > emulator-diagnostics/boot-logcat.txt || true
  exit 1
fi

adb install -r "$APK"
adb shell am force-stop com.davemega.hub
adb shell am start -W -n com.davemega.hub/.MainActivity
sleep 5
adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | grep -q "com.davemega.hub"
PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r')"
test -n "$PID"

# Select the Avatar tab using the active emulator's physical display metrics.
DISPLAY_SIZE="$(adb shell wm size | awk -F': ' '/Physical size/ { print $2; exit }' | tr -d '\r')"
SCREEN_W="${DISPLAY_SIZE%x*}"
SCREEN_H="${DISPLAY_SIZE#*x}"
DENSITY="$(adb shell wm density | awk -F': ' '/Physical density/ { print $2; exit }' | tr -d '\r')"
test -n "$SCREEN_W" && test -n "$SCREEN_H" && test -n "$DENSITY"
AVATAR_X=$((SCREEN_W * 3 / 10))
AVATAR_Y=$((SCREEN_H - (64 * DENSITY / 160)))
adb shell input tap "$AVATAR_X" "$AVATAR_Y"

# Wait for actual GLB load and a real Filament frame, not just Activity launch.
GLB_LOADED=0
for attempt in $(seq 1 60); do
  adb logcat --pid="$PID" -d > emulator-diagnostics/app-logcat.txt
  if grep -q "GLB loaded and GPU resources submitted" emulator-diagnostics/app-logcat.txt; then
    GLB_LOADED=1
    break
  fi
  sleep 2
done
if [[ "$GLB_LOADED" != "1" ]]; then
  echo "Avatar Studio did not report a successful GLB load."
  adb logcat -d > emulator-diagnostics/full-logcat.txt || true
  exit 1
fi

CURRENT_PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r')"
test "$CURRENT_PID" = "$PID"
if grep -E "FATAL EXCEPTION|Fatal signal|Process: com.davemega.hub" emulator-diagnostics/app-logcat.txt; then
  echo "Application crash detected in app-scoped emulator logcat."
  exit 1
fi

adb shell run-as com.davemega.hub cat shared_prefs/renderer_diagnostics.xml > emulator-diagnostics/renderer-preferences.xml
grep -q "frame submitted" emulator-diagnostics/renderer-preferences.xml
adb exec-out screencap -p > emulator-diagnostics/avatar-studio-screen.png
test -s emulator-diagnostics/avatar-studio-screen.png
{
  printf 'INSTALL=PASS\nLAUNCH=PASS\nAVATAR_STUDIO=OPENED\nGLB_LOAD=PASS\nRENDER_FRAME=SUBMITTED\nPROCESS_ID=%s\n' "$PID"
  printf 'SDK=%s\nABI=%s\nOPENGL_ES=%s\n' \
    "$(adb shell getprop ro.build.version.sdk | tr -d '\r')" \
    "$(adb shell getprop ro.product.cpu.abi | tr -d '\r')" \
    "$(adb shell getprop ro.opengles.version | tr -d '\r')"
} > emulator-diagnostics/result.txt

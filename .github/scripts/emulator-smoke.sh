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
adb shell am start -W -n com.davemega.hub/.MainActivity --ez com.davemega.hub.extra.OPEN_AVATAR_STUDIO true
sleep 5
adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | grep -q "com.davemega.hub"
PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r')"
test -n "$PID"

# Avatar Studio is opened by a deterministic launch extra; no coordinate taps required.

# Wait for actual GLB load and a real Filament frame, not just Activity launch.
GLB_LOADED=0
for attempt in $(seq 1 60); do
  adb logcat -b all -d > emulator-diagnostics/app-logcat.txt
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
adb shell dumpsys activity lastanr > emulator-diagnostics/lastanr.txt 2>&1 || true

if grep -E "FATAL EXCEPTION|Fatal signal|Process: com.davemega.hub" emulator-diagnostics/app-logcat.txt; then
  echo "Application crash detected in emulator logcat."
  exit 1
fi

# A renderer smoke test must reject an ANR or a sustained UI stall, even if the GLB
# eventually loads. The previous smoke script only checked for a late frame submission.
if grep -E "ANR in com\.davemega\.hub|Input dispatching timed out|Skipped ([3-9][0-9]{2,}|[1-9][0-9]{3,}) frames|Davey! duration=([1-9][0-9]{4,})ms" emulator-diagnostics/app-logcat.txt; then
  echo "ANR or sustained main-thread frame stall detected."
  exit 1
fi
if grep -q "com\.davemega\.hub" emulator-diagnostics/lastanr.txt; then
  echo "Android reports a previous ANR for 3D Mega Hub."
  exit 1
fi

adb shell run-as com.davemega.hub cat shared_prefs/renderer_diagnostics.xml > emulator-diagnostics/renderer-preferences.xml
grep -q "Filament OpenGL ES frame submitted" emulator-diagnostics/renderer-preferences.xml
adb exec-out screencap -p > emulator-diagnostics/avatar-studio-screen.png
test -s emulator-diagnostics/avatar-studio-screen.png
{
  printf 'INSTALL=PASS\nLAUNCH=PASS\nAVATAR_STUDIO=OPENED\nGLB_LOAD=PASS\nRENDER_FRAME=SUBMITTED\nPROCESS_ID=%s\n' "$PID"
  printf 'SDK=%s\nABI=%s\nOPENGL_ES=%s\n' \
    "$(adb shell getprop ro.build.version.sdk | tr -d '\r')" \
    "$(adb shell getprop ro.product.cpu.abi | tr -d '\r')" \
    "$(adb shell getprop ro.opengles.version | tr -d '\r')"
} > emulator-diagnostics/result.txt

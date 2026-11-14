#!/usr/bin/env bash
set -euo pipefail

mkdir -p emulator-diagnostics
# SMOKE_APK selects the artifact under test; the default is the debug APK built by this job.
APK="${SMOKE_APK:-app/build/outputs/apk/debug/app-debug.apk}"
test -s "$APK"
echo "Emulator smoke APK: $APK ($(stat -c%s "$APK") bytes, sha256 $(sha256sum "$APK" | cut -d' ' -f1))"

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

# A differently signed build of the same package may already be installed on a reused emulator.
adb uninstall com.davemega.hub >/dev/null 2>&1 || true
adb install -r "$APK"
adb shell am force-stop com.davemega.hub
adb shell am start -W -n com.davemega.hub/.MainActivity --ez com.davemega.hub.extra.OPEN_AVATAR_STUDIO true
sleep 5
adb shell dumpsys activity activities > emulator-diagnostics/activities.txt 2>&1 || true
PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r')"
if [[ -z "$PID" ]]; then
  echo "MainActivity launch returned successfully, but the app process is not running."
  adb logcat -b all -d > emulator-diagnostics/full-logcat.txt || true
  exit 1
fi

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

if grep -q "com\.davemega\.hub" emulator-diagnostics/lastanr.txt; then
  echo "Android reports a previous ANR for 3D Mega Hub."
  exit 1
fi

# GLB import and GPU submission precede the first presented frame. Wait for the
# renderer-owned checkpoint instead of sampling logcat once and racing the render thread.
RENDERER_FRAME=0
for attempt in $(seq 1 60); do
  adb logcat -b all -d > emulator-diagnostics/app-logcat.txt
  if grep -q "Renderer checkpoint: Filament OpenGL ES frame submitted" emulator-diagnostics/app-logcat.txt; then
    RENDERER_FRAME=1
    break
  fi
  sleep 1
done
if [[ "$RENDERER_FRAME" != "1" ]]; then
  echo "GLB loaded, but no successful Filament frame checkpoint appeared within 60 seconds."
  adb logcat -b all -d > emulator-diagnostics/full-logcat.txt || true
  exit 1
fi

# A renderer smoke test must reject an ANR or a sustained UI stall, even if the GLB
# eventually loads. The previous smoke script only checked for a late frame submission.
# Cold start, class loading and first GLB upload can create a one-off Choreographer
# backlog on a software-rendered CI emulator. Do not misclassify that startup burst as
# a renderer-time ANR. Begin the stall observation only after the GLB/frame checkpoint.
adb logcat -c
sleep 10
adb logcat -b all -d > emulator-diagnostics/post-render-logcat.txt
if grep -E "ANR in |Input dispatching timed out|Skipped ([3-9][0-9]{2,}|[1-9][0-9]{3,}) frames|Davey! duration=([1-9][0-9]{4,})ms" emulator-diagnostics/post-render-logcat.txt; then
  echo "ANR or sustained main-thread frame stall detected after renderer readiness."
  exit 1
fi
if grep -E "FATAL EXCEPTION|Fatal signal|Process: com.davemega.hub" emulator-diagnostics/post-render-logcat.txt; then
  echo "Application crash detected after renderer readiness."
  exit 1
fi
POST_PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r')"
if [[ -z "$POST_PID" || "$POST_PID" != "$PID" ]]; then
  echo "Application process died or restarted during the post-render observation window."
  exit 1
fi
if ! adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | grep -q "com.davemega.hub"; then
  echo "3D-Mega-Hub stopped being the resumed app during the post-render observation window."
  exit 1
fi
adb exec-out screencap -p > emulator-diagnostics/avatar-studio-screen.png
test -s emulator-diagnostics/avatar-studio-screen.png
{
  printf 'APK=%s\nAPK_SHA256=%s\n' "$APK" "$(sha256sum "$APK" | cut -d' ' -f1)"
  printf 'INSTALL=PASS\nLAUNCH=PASS\nAVATAR_STUDIO=OPENED\nGLB_LOAD=PASS\nRENDER_FRAME=SUBMITTED\nPROCESS_ID=%s\n' "$PID"
  printf 'SDK=%s\nABI=%s\nOPENGL_ES=%s\n' \
    "$(adb shell getprop ro.build.version.sdk | tr -d '\r')" \
    "$(adb shell getprop ro.product.cpu.abi | tr -d '\r')" \
    "$(adb shell getprop ro.opengles.version | tr -d '\r')"
} > emulator-diagnostics/result.txt
cat emulator-diagnostics/result.txt

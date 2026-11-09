#!/usr/bin/env bash
set -uo pipefail

mkdir -p avatarforge-render-smoke-evidence
chmod +x ./gradlew

TEST_STATUS=0
./gradlew --no-daemon :app:connectedDebugAndroidTest --stacktrace || TEST_STATUS=$?

adb logcat -d -v threadtime > avatarforge-render-smoke-evidence/logcat.txt 2>&1 || true
adb logcat -d -v threadtime -s AvatarForge:V > avatarforge-render-smoke-evidence/avatarforge-logcat.txt 2>&1 || true
adb shell dumpsys activity activities > avatarforge-render-smoke-evidence/activities.txt 2>&1 || true
adb shell run-as com.davemega.hub cat shared_prefs/avatarforge_renderer_smoke.xml > avatarforge-render-smoke-evidence/renderer-state.xml 2>&1 || true
adb exec-out screencap -p > avatarforge-render-smoke-evidence/final-screen.png 2>/dev/null || true
adb pull /sdcard/Android/data/com.davemega.hub/files/avatarforge-renderer-smoke.png avatarforge-render-smoke-evidence/avatarforge-viewport.png >/dev/null 2>&1 || true
adb pull /sdcard/Android/data/com.davemega.hub/files/failed_render.png avatarforge-render-smoke-evidence/failed_render.png >/dev/null 2>&1 || true

if [[ "$TEST_STATUS" -ne 0 ]]; then
  echo "FAIL: instrumented AvatarForge GLES GLB renderer test failed with exit $TEST_STATUS."
  exit "$TEST_STATUS"
fi

PID="$(adb shell pidof -s com.davemega.hub | tr -d '\r' || true)"
if [[ -z "$PID" ]]; then
  echo "FAIL: app process is not alive after renderer instrumentation."
  exit 1
fi
if ! adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" | grep -q "com.davemega.hub"; then
  echo "FAIL: 3D-Mega-Hub is not the resumed app after renderer instrumentation."
  exit 1
fi
if ! grep -q 'FRAME_VERIFIED' avatarforge-render-smoke-evidence/renderer-state.xml; then
  echo "FAIL: persisted AvatarForge renderer state does not contain FRAME_VERIFIED."
  exit 1
fi
for marker in "GLB accessor semantic=POSITION" "GLB packed vertex pool" "af_vertex layout" "GPU Vertex 0" "MVP clip vertex 0" "DrawElements submesh="; do
  if ! grep -Fq "$marker" avatarforge-render-smoke-evidence/avatarforge-logcat.txt; then
    echo "FAIL: AvatarForge logcat evidence is missing diagnostic marker: $marker"
    exit 1
  fi
done
if [[ ! -s avatarforge-render-smoke-evidence/avatarforge-viewport.png ]]; then
  echo "FAIL: renderer screenshot artifact is missing."
  exit 1
fi
echo "PASS: instrumented AvatarForge GLES GLB render test completed; app PID=$PID."

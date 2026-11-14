#!/usr/bin/env bash
set -uo pipefail

mkdir -p avatarforge-render-smoke-evidence
chmod +x ./gradlew

TEST_STATUS=0
./gradlew --no-daemon :app:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.davemega.hub.AvatarForgeRendererSmokeTest --stacktrace || TEST_STATUS=$?

# connectedDebugAndroidTest tears down the instrumented target/test packages after
# the suite. The app process and app-private files are therefore not valid
# postconditions here. The renderer tests assert PixelCopy success and screenshot
# creation while the app is installed; the app also exports durable evidence to
# shared Downloads before instrumentation cleanup.
adb logcat -d -v threadtime > avatarforge-render-smoke-evidence/logcat.txt 2>&1 || true
adb logcat -d -v threadtime -s AvatarForge:V > avatarforge-render-smoke-evidence/avatarforge-logcat.txt 2>&1 || true
adb shell dumpsys activity activities > avatarforge-render-smoke-evidence/activities.txt 2>&1 || true
adb exec-out screencap -p > avatarforge-render-smoke-evidence/final-screen.png 2>/dev/null || true

DIAGNOSTICS_DIR="/sdcard/Download/3D Mega Hub Diagnostics"
adb pull "$DIAGNOSTICS_DIR/renderer_diagnostics.json" avatarforge-render-smoke-evidence/renderer_diagnostics.json >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/imported_glb_diagnostic.png" avatarforge-render-smoke-evidence/imported_glb_diagnostic.png >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/generated_avatar_diagnostic.png" avatarforge-render-smoke-evidence/generated_avatar_diagnostic.png >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/failed_render.png" avatarforge-render-smoke-evidence/failed_render.png >/dev/null 2>&1 || true
adb pull "$DIAGNOSTICS_DIR/renderer_telemetry.json" avatarforge-render-smoke-evidence/renderer_telemetry.json >/dev/null 2>&1 || true

# Put the measured renderer evidence in the job log itself so it can be read without the artifact.
echo "::group::Measured renderer evidence (logcat, tag AvatarForge)"
grep -E "TELEMETRY |EVIDENCE|SLOW_START|STALL |FRAME_PRESENTED|Renderer failed|Avatar LOD" \
  avatarforge-render-smoke-evidence/avatarforge-logcat.txt | sed -E 's/^[0-9-]+ +[0-9:.]+ +[0-9]+ +[0-9]+ +//' | tail -n 60 || true
echo "::endgroup::"
echo "::group::Crashes, ANRs and fatal signals seen by the emulator"
grep -E "FATAL EXCEPTION|Fatal signal|ANR in com\.davemega\.hub|Process: com\.davemega\.hub" avatarforge-render-smoke-evidence/logcat.txt | tail -n 20 \
  || echo "none found in logcat"
echo "::endgroup::"

if [[ "$TEST_STATUS" -ne 0 ]]; then
  echo "FAIL: instrumented AvatarForge GLES GLB renderer test failed with exit $TEST_STATUS."
  exit "$TEST_STATUS"
fi

if ! grep -Eq 'run finished: 3 tests, 0 failed, 0 ignored' avatarforge-render-smoke-evidence/logcat.txt; then
  echo "FAIL: instrumentation log does not prove all three renderer tests passed."
  exit 1
fi
if ! grep -q '"status"[[:space:]]*:[[:space:]]*"PIXELS_VERIFIED"' avatarforge-render-smoke-evidence/renderer_diagnostics.json; then
  echo "FAIL: persisted renderer JSON does not prove PIXELS_VERIFIED."
  exit 1
fi
for marker in "GLB accessor semantic=POSITION" "GLB packed vertex pool" "af_vertex layout" "GPU Vertex 0" "MVP clip vertex 0" "DrawElements submesh="; do
  if ! grep -Fq "$marker" avatarforge-render-smoke-evidence/avatarforge-logcat.txt; then
    echo "FAIL: AvatarForge logcat evidence is missing diagnostic marker: $marker"
    exit 1
  fi
done
if [[ ! -s avatarforge-render-smoke-evidence/imported_glb_diagnostic.png ]]; then
  echo "FAIL: durable imported-GLB PixelCopy screenshot is missing."
  exit 1
fi
if [[ -s avatarforge-render-smoke-evidence/failed_render.png ]]; then
  echo "FAIL: renderer exported failed_render.png; inspect the captured failure."
  exit 1
fi
# Camera interaction and native lifetime are asserted inside the tests; require their evidence
# lines too, so a silently skipped assertion cannot pass this gate.
CAMERA_LINES="$(grep -c "CAMERA_EVIDENCE " avatarforge-render-smoke-evidence/avatarforge-logcat.txt || true)"
if [[ "${CAMERA_LINES:-0}" -lt 2 ]]; then
  echo "FAIL: expected camera-orbit evidence from both imported-GLB tests, found ${CAMERA_LINES:-0}."
  exit 1
fi
RELEASED_LINES="$(grep -c -E "SESSION_EVIDENCE .* live=0([^0-9]|$)" avatarforge-render-smoke-evidence/avatarforge-logcat.txt || true)"
if [[ "${RELEASED_LINES:-0}" -lt 2 ]]; then
  echo "FAIL: expected native-session release evidence (live=0) from both imported-GLB tests, found ${RELEASED_LINES:-0}."
  exit 1
fi
if [[ ! -s avatarforge-render-smoke-evidence/renderer_telemetry.json ]]; then
  echo "FAIL: the renderer did not persist renderer_telemetry.json at the end of a render session."
  exit 1
fi
echo "PASS: all three instrumented renderer tests passed; imported GLB PixelCopy, camera-orbit, native-lifetime and measured telemetry evidence captured."

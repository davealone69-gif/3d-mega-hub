#!/usr/bin/env bash
# Checks, on a machine without the Android NDK, the native code that only an Android build
# compiles. It saves a CI round trip (about a quarter of an hour) for a typing mistake.
#
#   tools/shader_check/check_android_native.sh [WORKDIR]      (default /tmp/af-native-check)
#
# 1. GLSL: every shader embedded in af_shaders_gl.h is compiled with glslangValidator (the
#    Khronos reference front end, from the npm package glslang-validator-prebuilt-predownloaded),
#    and each program's vertex and fragment shader are linked.
# 2. C++: af_gl.cpp, af_render.cpp, af_render_capi.cpp and avatarforge_renderer_jni.cpp are
#    parsed with g++ -fsyntax-only -Wall -Wextra against the Khronos GLES/EGL headers, OpenJDK's
#    jni.h and three small stand-ins for the Android headers. Nothing is linked or run.
#
# What it does NOT show: that a driver accepts the shaders, or that anything draws. The emulator
# jobs in CI show that.
#
# Needs g++, python3, node with npm, and the GitHub CLI signed in (the headers are fetched from
# KhronosGroup/OpenGL-Registry, KhronosGroup/EGL-Registry and openjdk/jdk; none is kept in this
# repository).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CPP="$ROOT/avatarforge-engine/src/main/cpp"
WORK="${1:-/tmp/af-native-check}"
mkdir -p "$WORK/glsl" "$WORK/include/GLES3" "$WORK/include/GLES2" "$WORK/include/EGL" "$WORK/include/KHR" "$WORK/include/android"

echo "== GLSL"
if [ ! -x "$WORK/node_modules/glslang-validator-prebuilt-predownloaded/bin/glslangValidator.linux" ]; then
    (cd "$WORK" && npm init -y >/dev/null && npm install --silent glslang-validator-prebuilt-predownloaded >/dev/null)
    chmod +x "$WORK/node_modules/glslang-validator-prebuilt-predownloaded/bin/glslangValidator.linux"
fi
GLSLANG="$WORK/node_modules/glslang-validator-prebuilt-predownloaded/bin/glslangValidator.linux"
rm -f "$WORK"/glsl/*
python3 -B "$ROOT/tools/shader_check/extract_shaders.py" "$CPP/src/render/af_shaders_gl.h" "$WORK/glsl" >/dev/null
failed=0
for shader in "$WORK"/glsl/*; do
    if ! "$GLSLANG" "$shader" >"$WORK/glslang.log" 2>&1 || grep -q "ERROR" "$WORK/glslang.log"; then
        cat "$WORK/glslang.log"; failed=1
    fi
done
link() {
    if ! "$GLSLANG" -l "$WORK/glsl/$1" "$WORK/glsl/$2" >"$WORK/glslang.log" 2>&1 || grep -q "ERROR" "$WORK/glslang.log"; then
        cat "$WORK/glslang.log"; failed=1
    fi
}
link forward.vert forward.frag
link shadow.vert shadow.frag
link ground.vert ground.frag
link line.vert line.frag
link fullscreen.vert bloom_down.frag
link fullscreen.vert bloom_up.frag
link fullscreen.vert composite.frag
[ "$failed" = 0 ] || { echo "GLSL: FAILED"; exit 1; }
echo "GLSL: $(ls "$WORK/glsl" | wc -l) shaders compile, 7 programs link"

echo "== C++ (syntax only)"
fetch() {   # repository, path in it, destination
    [ -s "$3" ] || gh api -H "Accept: application/vnd.github.raw" "repos/$1/contents/$2" > "$3"
}
I="$WORK/include"
fetch KhronosGroup/OpenGL-Registry api/GLES3/gl31.h "$I/GLES3/gl31.h"
fetch KhronosGroup/OpenGL-Registry api/GLES3/gl3platform.h "$I/GLES3/gl3platform.h"
fetch KhronosGroup/OpenGL-Registry api/GLES2/gl2ext.h "$I/GLES2/gl2ext.h"
fetch KhronosGroup/OpenGL-Registry api/GLES2/gl2platform.h "$I/GLES2/gl2platform.h"
fetch KhronosGroup/EGL-Registry api/EGL/egl.h "$I/EGL/egl.h"
fetch KhronosGroup/EGL-Registry api/EGL/eglext.h "$I/EGL/eglext.h"
fetch KhronosGroup/EGL-Registry api/EGL/eglplatform.h "$I/EGL/eglplatform.h"
fetch KhronosGroup/EGL-Registry api/KHR/khrplatform.h "$I/KHR/khrplatform.h"
fetch openjdk/jdk src/java.base/share/native/include/jni.h "$I/jni.h"
fetch openjdk/jdk src/java.base/unix/native/include/jni_md.h "$I/jni_md.h"
cat > "$I/android/native_window.h" <<'STUB'
/* Stand-in for a syntax check on a machine without the NDK. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;
void ANativeWindow_acquire(ANativeWindow*);
void ANativeWindow_release(ANativeWindow*);
int32_t ANativeWindow_getWidth(ANativeWindow*);
int32_t ANativeWindow_getHeight(ANativeWindow*);
int32_t ANativeWindow_getFormat(ANativeWindow*);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow*, int32_t, int32_t, int32_t);
#ifdef __cplusplus
}
#endif
STUB
cat > "$I/android/native_window_jni.h" <<'STUB'
#pragma once
#include <jni.h>
#include <android/native_window.h>
#ifdef __cplusplus
extern "C" {
#endif
ANativeWindow* ANativeWindow_fromSurface(JNIEnv*, jobject);
#ifdef __cplusplus
}
#endif
STUB
cat > "$I/android/log.h" <<'STUB'
#pragma once
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { ANDROID_LOG_UNKNOWN = 0, ANDROID_LOG_DEFAULT, ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO,
       ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_FATAL, ANDROID_LOG_SILENT };
int __android_log_print(int prio, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
int __android_log_write(int prio, const char* tag, const char* text);
#ifdef __cplusplus
}
#endif
STUB
failed=0
for source in src/render/af_gl.cpp src/render/af_render.cpp src/render/af_render_capi.cpp avatarforge_renderer_jni.cpp; do
    if ! (cd "$CPP" && g++ -std=c++17 -fsyntax-only -Wall -Wextra -D__ANDROID__ -DANDROID \
            -I"$I" -Iinclude -Isrc -Isrc/render "$source") 2>"$WORK/gxx.log"; then
        failed=1
    fi
    if [ -s "$WORK/gxx.log" ]; then echo "-- $source"; cat "$WORK/gxx.log"; fi
done
[ "$failed" = 0 ] || { echo "C++: FAILED"; exit 1; }
echo "C++: four Android-only sources parse"

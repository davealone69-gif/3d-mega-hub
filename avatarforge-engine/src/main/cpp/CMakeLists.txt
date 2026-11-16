cmake_minimum_required(VERSION 3.22)
project(mega_hub_avatarforge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep AvatarForge's numerical safeguards: do not use fast-math for geometry.
set(AF_OPT_FLAGS "-O3" CACHE STRING "AvatarForge core optimization flags")
set(AF_KERNEL_FLAGS ${AF_OPT_FLAGS} -fno-math-errno -fno-finite-math-only -ffp-contract=off)

# Animation core (src/anim): renderer-agnostic clip sampling, blending, state machine, layers,
# morph/facial control, lip-sync and retargeting. Pure CPU, no graphics or Android headers, so it
# builds and is tested identically on the host and in the NDK.
set(AF_ANIM_SOURCES
    src/anim/af_anim_skeleton.cpp
    src/anim/af_anim_clip.cpp
    src/anim/af_anim_pose.cpp
    src/anim/af_anim_statemachine.cpp
    src/anim/af_anim_retarget.cpp
    src/anim/af_anim_face.cpp
    src/anim/af_anim_player.cpp
    src/anim/af_anim_library.cpp)

add_library(afcore SHARED
    src/af_anatomy.cpp
    ${AF_ANIM_SOURCES}
    src/af_body.cpp
    src/render/af_avatar_lod.cpp
    src/af_engine.cpp
    src/af_field.cpp
    src/af_glb.cpp
    src/af_glb_read.cpp
    src/af_json.cpp
    src/af_image.cpp
    src/af_isosurface.cpp
    src/af_png.cpp
    src/af_png_decode.cpp
    src/af_prompt.cpp
    src/af_rig.cpp
    src/af_scene.cpp
    src/af_scene_bridge.cpp
    src/af_profiler.cpp
    src/af_spec.cpp
    src/af_texture.cpp
    src/af_uv.cpp)
target_include_directories(afcore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(afcore PRIVATE ${AF_KERNEL_FLAGS} -Wall -Wextra)
target_link_libraries(afcore PRIVATE z ${CMAKE_DL_LIBS})
if(ANDROID)
    # AImageDecoder is API 30+; symbols are resolved dynamically to preserve minSdk 26.
    target_link_libraries(afcore PRIVATE dl log)
endif()

# AvatarForge custom renderer, copied from the canonical engine source.
# GLES is forced by the Hub runtime on the target Samsung profile; Vulkan remains
# compiled as an optional backend but is not selected by default.
if(ANDROID)
    add_library(afrender SHARED
        src/render/af_render.cpp
        src/render/af_render_capi.cpp
        src/render/af_gl.cpp
        src/render/af_vk.cpp
        src/render/af_shaders.cpp)
    target_include_directories(afrender
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
    target_compile_options(afrender PRIVATE ${AF_OPT_FLAGS} -Wall -Wextra)
    target_link_libraries(afrender PUBLIC afcore PRIVATE vulkan EGL GLESv3 android log)
endif()
set_target_properties(afcore PROPERTIES VERSION "" SOVERSION "" PREFIX "lib")

if(ANDROID)
    add_library(mega_hub_avatarforge SHARED
        avatarforge_jni.cpp
        avatarforge_renderer_jni.cpp
        avatarforge_animation_jni.cpp)
    target_include_directories(mega_hub_avatarforge PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
    target_compile_options(mega_hub_avatarforge PRIVATE -O2 -Wall -Wextra)
    target_link_libraries(mega_hub_avatarforge PRIVATE afcore afrender android log)
endif()

# Host-side native regression test. Kept out of Android NDK builds.
if(BUILD_TESTING AND NOT ANDROID)
    find_package(Threads REQUIRED)
    enable_testing()
    add_executable(af_native_sampling_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/native_sampling_test.cpp)
    target_include_directories(af_native_sampling_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(af_native_sampling_test PRIVATE afcore Threads::Threads)
    add_test(NAME native_sampling_regression COMMAND af_native_sampling_test)

    add_executable(af_glb_extension_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/glb_extension_test.cpp)
    target_include_directories(af_glb_extension_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_glb_extension_test PRIVATE afcore)
    add_test(NAME glb_extension_regression COMMAND af_glb_extension_test)

    # Camera math uses a separate translation unit because af_math.h and
    # af_render_common.h intentionally own distinct renderer/kernel math types.
    add_executable(af_camera_frustum_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/camera_frustum_test.cpp)
    target_include_directories(af_camera_frustum_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_camera_frustum_test PRIVATE afcore)
    add_test(NAME camera_frustum_regression COMMAND af_camera_frustum_test)

    add_executable(af_avatar_lod_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/avatar_lod_test.cpp)
    target_include_directories(af_avatar_lod_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_avatar_lod_test PRIVATE afcore)
    add_test(NAME avatar_lod_regression COMMAND af_avatar_lod_test)

    add_executable(af_scene_visibility_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/scene_visibility_test.cpp)
    target_include_directories(af_scene_visibility_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(af_scene_visibility_test PRIVATE afcore)
    add_test(NAME scene_visibility_regression COMMAND af_scene_visibility_test)

    # Animation system end-to-end regression. Runs against the real Khronos glTF sample assets the
    # app bundles, so the asset directory is baked in at configure time and can be overridden by
    # passing a directory as the executable's first argument.
    # Not a CACHE variable: a cached default goes stale when the tree moves. Override on the command
    # line with -DAF_ANIMATION_TEST_ASSETS=/path/to/glbs if the fixtures live elsewhere.
    # Default to the fixtures the app actually ships, so the host test proves the very files that end
    # up in the APK rather than a second copy that could drift from them. Override with
    # -DAF_ANIMATION_TEST_ASSETS=/path/to/glbs when building the engine module on its own.
    if(NOT DEFINED AF_ANIMATION_TEST_ASSETS)
        set(AF_ANIMATION_TEST_ASSETS
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../../app/src/main/assets/models/animation")
    endif()
    if(NOT EXISTS "${AF_ANIMATION_TEST_ASSETS}/Fox.glb")
        message(WARNING
            "animation test assets not found at ${AF_ANIMATION_TEST_ASSETS}; "
            "animation_system_regression will fail until Fox.glb, CesiumMan.glb and "
            "AnimatedMorphCube.glb are present there (see tools/fetch_animation_assets.py)")
    endif()
    add_executable(af_animation_system_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/animation_system_test.cpp)
    target_include_directories(af_animation_system_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(af_animation_system_test PRIVATE
        AF_TEST_ASSET_DIR="${AF_ANIMATION_TEST_ASSETS}")
    target_link_libraries(af_animation_system_test PRIVATE afcore)
    add_test(NAME animation_system_regression COMMAND af_animation_system_test)

    add_executable(af_imported_glb_pbr_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/imported_glb_pbr_test.cpp)
    target_include_directories(af_imported_glb_pbr_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_imported_glb_pbr_test PRIVATE afcore)
    add_test(NAME imported_glb_pbr_regression COMMAND af_imported_glb_pbr_test)

    # Proves an imported GLB is actually animated: the skeleton and clips it carries are adopted and
    # its vertex buffer is posed. Guards the path that used to parse both and then discard them.
    add_executable(af_imported_animation_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/imported_animation_test.cpp)
    target_include_directories(af_imported_animation_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_definitions(af_imported_animation_test PRIVATE
        AF_TEST_ASSET_DIR="${AF_ANIMATION_TEST_ASSETS}")
    target_link_libraries(af_imported_animation_test PRIVATE afcore)
    add_test(NAME imported_animation_regression
             COMMAND af_imported_animation_test "${AF_ANIMATION_TEST_ASSETS}")
endif()


cmake_minimum_required(VERSION 3.22)
project(mega_hub_avatarforge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep AvatarForge's numerical safeguards: do not use fast-math for geometry.
set(AF_OPT_FLAGS "-O3" CACHE STRING "AvatarForge core optimization flags")
set(AF_KERNEL_FLAGS ${AF_OPT_FLAGS} -fno-math-errno -fno-finite-math-only -ffp-contract=off)

add_library(afcore SHARED
    src/af_anatomy.cpp
    src/af_body.cpp
    src/render/af_avatar_lod.cpp
    src/af_engine.cpp
    src/af_face.cpp
    src/af_field.cpp
    src/af_glb.cpp
    src/af_glb_read.cpp
    src/af_json.cpp
    src/af_image.cpp
    src/af_isosurface.cpp
    src/af_png.cpp
    src/af_png_decode.cpp
    src/af_prompt.cpp
    src/af_retarget.cpp
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
# The Hub renders with OpenGL ES only: both JNI entry points set preferVulkan = false and no
# code path falls back to Vulkan. The Vulkan backend source (af_vk.cpp) stays in the tree, but it
# is not compiled into libafrender.so and the library does not link libvulkan.so unless
# -DAF_ENABLE_VULKAN=ON is passed. The release gate reads the library's NEEDED entries back.
option(AF_ENABLE_VULKAN "Build the Vulkan backend into libafrender (the Hub never selects it)" OFF)
if(ANDROID)
    set(AF_RENDER_SOURCES
        src/render/af_render.cpp
        src/render/af_render_capi.cpp
        src/render/af_gl.cpp
        src/render/af_shaders.cpp)
    set(AF_RENDER_SYSTEM_LIBS EGL GLESv3 android log)
    if(AF_ENABLE_VULKAN)
        list(APPEND AF_RENDER_SOURCES src/render/af_vk.cpp)
        list(APPEND AF_RENDER_SYSTEM_LIBS vulkan)
    endif()
    add_library(afrender SHARED ${AF_RENDER_SOURCES})
    target_include_directories(afrender
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
    target_compile_options(afrender PRIVATE ${AF_OPT_FLAGS} -Wall -Wextra)
    if(AF_ENABLE_VULKAN)
        target_compile_definitions(afrender PRIVATE AF_ENABLE_VULKAN=1)
    endif()
    target_link_libraries(afrender PUBLIC afcore PRIVATE ${AF_RENDER_SYSTEM_LIBS})
endif()
set_target_properties(afcore PROPERTIES VERSION "" SOVERSION "" PREFIX "lib")

if(ANDROID)
    add_library(mega_hub_avatarforge SHARED
        avatarforge_jni.cpp
        avatarforge_renderer_jni.cpp)
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

    add_executable(af_imported_glb_pbr_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/imported_glb_pbr_test.cpp)
    target_include_directories(af_imported_glb_pbr_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/render
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_imported_glb_pbr_test PRIVATE afcore)
    add_test(NAME imported_glb_pbr_regression COMMAND af_imported_glb_pbr_test)

    # The importer must reject or accept every mutated file without crashing, and an accepted
    # file must never carry an index or submesh range outside its buffers.
    add_executable(af_glb_import_robustness_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/glb_import_robustness_test.cpp)
    target_include_directories(af_glb_import_robustness_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_glb_import_robustness_test PRIVATE afcore)
    add_test(NAME glb_import_robustness
        COMMAND af_glb_import_robustness_test
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../app/src/main/assets/models/female.glb 8000 1337)

    # Generates a draft avatar, exports it, imports the file into a second engine and checks that
    # the imported avatar can be animated and moves exactly as the generating engine's own rig.
    add_executable(af_animation_roundtrip_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/animation_roundtrip_test.cpp)
    target_include_directories(af_animation_roundtrip_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_animation_roundtrip_test PRIVATE afcore)
    add_test(NAME animation_roundtrip COMMAND af_animation_roundtrip_test)

    # The animation system on real data: clips, skin weights, blending and layers, retargeting
    # of a humanoid motion pack onto a generated avatar, and the face rig. The two GLB files are
    # the ones the instrumented tests open on Android.
    add_executable(af_animation_system_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/animation_system_test.cpp)
    target_include_directories(af_animation_system_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_animation_system_test PRIVATE afcore)
    add_test(NAME animation_system
        COMMAND af_animation_system_test
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../app/src/androidTest/assets)

    # One prompt per hair, outfit and footwear preset must resolve to that preset.
    add_executable(af_prompt_preset_words_test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/prompt_preset_words_test.cpp)
    target_include_directories(af_prompt_preset_words_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_prompt_preset_words_test PRIVATE afcore)
    add_test(NAME prompt_preset_words COMMAND af_prompt_preset_words_test)

    # Evidence probe: generates an avatar through the same sequence as the Android JNI layer and
    # measures orientation, mirror symmetry and the export/import round trip. It takes about a
    # minute, so it is built with the host tests but deliberately not registered with CTest.
    add_executable(af_host_probe
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/host_avatar_probe.cpp)
    target_include_directories(af_host_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_host_probe PRIVATE afcore)

    # Evidence tool: imports a GLB through the engine, reports what the engine kept of it (mesh,
    # materials, joints, animations) and draws the engine's buffers with a software rasteriser.
    # Not a test and not the app's renderer; see the header of the source file.
    add_executable(af_host_render
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/host_avatar_render.cpp)
    target_include_directories(af_host_render PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(af_host_render PRIVATE afcore)
endif()


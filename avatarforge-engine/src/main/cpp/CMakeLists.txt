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
    src/af_profiler.cpp
    src/af_spec.cpp
    src/af_texture.cpp
    src/af_uv.cpp)
target_include_directories(afcore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(afcore PRIVATE ${AF_KERNEL_FLAGS} -Wall -Wextra)
target_link_libraries(afcore PRIVATE z)
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
endif()

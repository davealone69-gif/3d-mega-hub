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
set_target_properties(afcore PROPERTIES VERSION "" SOVERSION "" PREFIX "lib")

add_library(mega_hub_avatarforge SHARED avatarforge_jni.cpp)
target_include_directories(mega_hub_avatarforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(mega_hub_avatarforge PRIVATE -O2 -Wall -Wextra)
target_link_libraries(mega_hub_avatarforge PRIVATE afcore)

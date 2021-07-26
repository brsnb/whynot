cmake_minimum_required(VERSION 3.8...3.20)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(NAME whynot)

project(${NAME} VERSION 0.0.1 LANGUAGES C)


set(SOURCES
    src/render/device.c
    src/render/shader_compile.c
    src/main.c)

set(HEADERS
    src/core/core_types.h
    src/core/file.inl
    src/core/math.inl
    src/render/device.h
    src/render/render_types.h
    src/render/shader_compile.h
    src/render/util_vk.inl
    src/render/vk.h)

add_executable(whynot ${HEADERS} ${SOURCES})

target_include_directories(${NAME} PUBLIC src/core src/render external/stb external/log.c/src ${ASSIMP_INCLUDE_DIRS})

target_link_libraries(${NAME} PRIVATE ${ASSIMP_LIBRARIES} m stdc++ SPIRV glslang shaderc_combined vulkan glfw )

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(ASSIMP REQUIRED)

target_compile_definitions(${NAME} PUBLIC LOG_USE_COLOR)
target_compile_features(${NAME} PUBLIC c_std_11)
target_compile_options(${NAME} PUBLIC -Wextra -Wall -Wshadow -Wno-missing-braces -Wmissing-field-initializers -fdiagnostics-color=always)


# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

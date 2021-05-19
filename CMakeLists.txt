cmake_minimum_required(VERSION 3.8...3.20)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(NAME whynot)

project(${NAME} VERSION 0.0.1 LANGUAGES C)

add_executable(whynot
    external/log.c/src/log.h
    external/log.c/src/log.c
    external/stb/stb_ds.h
    src/util.h
    src/main.c)

target_compile_definitions(${NAME} PUBLIC LOG_USE_COLOR)
target_compile_features(${NAME} PUBLIC c_std_11)
target_compile_options(${NAME} PUBLIC -Wextra -Wall -Wshadow -Wno-missing-braces -fdiagnostics-color=always)

#add_compile_options(-Wextra -Wall -fdiagnostics-color=always -DLOG_USE_COLOR -lstdc++)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

target_include_directories(${NAME} PUBLIC external/stb external/log.c/src)

target_link_libraries(${NAME} PRIVATE m stdc++ SPIRV glslang shaderc_combined vulkan glfw)

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

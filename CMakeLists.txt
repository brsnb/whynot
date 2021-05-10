cmake_minimum_required(VERSION 3.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(NAME whynot)

project(${NAME})

add_compile_options(-W -Wall -DLOG_USE_COLOR -lstdc++)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

include_directories(external/stb)
include_directories(external/log.c/src)

add_executable(${NAME}
    external/log.c/src/log.h
    external/log.c/src/log.c
    external/stb/stb_ds.h
    src/util.h
    src/main.c)

target_compile_features(${NAME} PUBLIC c_std_99)
target_link_libraries(${NAME} PRIVATE stdc++ SPIRV glslang shaderc_combined vulkan glfw)

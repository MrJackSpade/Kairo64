cmake_minimum_required(VERSION 3.22.1)
project(kairo64 LANGUAGES C CXX)

get_filename_component(KAIRO64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
include("${KAIRO64_ROOT}/shared/cmake/KairoMachine.cmake")

# The Commodore 64 on VICE, driven one frame at a time (core/).
add_subdirectory("${KAIRO64_ROOT}/core" core)

kairo_machine_library(kairo64 c64_core.cpp)
target_link_libraries(kairo64 PRIVATE kairo64_core)

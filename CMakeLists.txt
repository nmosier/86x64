cmake_minimum_required(VERSION 3.10)
project(86x64 VERSION 0.0)

enable_language(ASM_NASM)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# find_package(xed)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(scripts)

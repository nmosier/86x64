cmake_minimum_required(VERSION 3.10)

project(86x64 VERSION 0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(LIEF)
find_package(xed)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(c++)
add_subdirectory(src)


cmake_minimum_required(VERSION 3.10)
project(zc VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# required libraries & executables
find_package(BISON)
find_package(FLEX 2.5.35 REQUIRED)

add_compile_options("-pedantic")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)

add_subdirectory(ast)
add_subdirectory(semant)

add_library(zc_objs
  OBJECT
  env.cpp
)

add_executable(parser
  parser-main.cc
  $<TARGET_OBJECTS:zc_objs>
  $<TARGET_OBJECTS:ast_objs>
  $<TARGET_OBJECTS:semant_objs>
  )  

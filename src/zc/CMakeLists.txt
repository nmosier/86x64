cmake_minimum_required(VERSION 3.10)
project(zc VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# required libraries & executables
find_package(BISON)
find_package(FLEX 2.5.35 REQUIRED)

add_compile_options("-pedantic;-DEZ80")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)



add_subdirectory(ast)
add_subdirectory(asm)
add_subdirectory(cgen)
add_subdirectory(optim)
add_subdirectory(ralloc)
add_subdirectory(semant)
add_subdirectory(crt)
add_subdirectory(alg)

add_library(zc_objs
  OBJECT
  env.cpp
)

add_executable(zc
  cgen-main.cc
  $<TARGET_OBJECTS:zc_objs>
  $<TARGET_OBJECTS:ast_objs>
  $<TARGET_OBJECTS:asm_objs>
  $<TARGET_OBJECTS:cgen_objs>
  $<TARGET_OBJECTS:optim_objs>
  $<TARGET_OBJECTS:ralloc_objs>
  $<TARGET_OBJECTS:semant_objs>
  $<TARGET_OBJECTS:crt_objs>
  $<TARGET_OBJECTS:alg_objs>
)

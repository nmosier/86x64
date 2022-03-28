include(ExternalProject)
include(FetchContent)

ExternalProject_Add(mbuild
  PREFIX xed
  GIT_REPOSITORY https://github.com/intelxed/mbuild.git
  GIT_TAG main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

find_package(Python3 REQUIRED)

ExternalProject_Add(xed
  PREFIX xed
  GIT_REPOSITORY https://github.com/intelxed/xed.git
  GIT_TAG main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/build-xed.sh" "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed" "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed-build"
# BUILD_COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed-build"
# COMMAND "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed/mfile.py" --jobs=8 > /dev/null
  INSTALL_COMMAND ""
  DEPENDS mbuild
  )

ExternalProject_Get_Property(xed BINARY_DIR)
set(xed_BINARY_DIR "${BINARY_DIR}")
set(xed_LIBRARIES "${xed_BINARY_DIR}/obj/libxed.a" CACHE INTERNAL "xed_LIBRARIES")
set(xed_INCLUDE_DIRS "${xed_BINARY_DIR}/obj/wkit/include" CACHE INTERNAL "xed_INCLUDE_DIRS")

# FetchContent_Declare(yaml_cpp
#   GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
# )
# FetchContent_MakeAvailable(yaml_cpp)

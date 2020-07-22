include(ExternalProject)

ExternalProject_Add(mbuild
  PREFIX xed
  GIT_REPOSITORY https://github.com/intelxed/mbuild.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

find_package(Python3 REQUIRED)

ExternalProject_Add(xed
  PREFIX xed
  GIT_REPOSITORY https://github.com/intelxed/xed.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed/mfile.py" --install-dir="${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed-build" --jobs=8 install
  INSTALL_COMMAND ""
  DEPENDS mbuild
  )

ExternalProject_Get_Property(xed BINARY_DIR)
set(xed_BINARY_DIR "${BINARY_DIR}")
set(xed_LIBRARIES "${xed_BINARY_DIR}/lib/libxed.a" CACHE INTERNAL "xed_LIBRARIES")
set(xed_INCLUDE_DIRS "${xed_BINARY_DIR}/include" CACHE INTERNAL "xed_INCLUDE_DIRS")

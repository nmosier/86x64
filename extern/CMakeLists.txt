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
  BUILD_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/xed/src/xed/mfile.py" --install-dir="xed/src/xed-build" --jobs=8 install
  INSTALL_COMMAND ""
  DEPENDS mbuild
  )


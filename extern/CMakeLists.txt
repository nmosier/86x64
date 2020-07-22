include(ExternalProject)

ExternalProject_Add(mbuild
  GIT_REPOSITORY https://github.com/intelxed/mbuild.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

find_package(Python3 REQUIRED)

ExternalProject_Add(xed
  GIT_REPOSITORY https://github.com/intelxed/xed.git
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/xed/mfile.py" --install-dir="${CMAKE_CURRENT_BINARY_DIR}/xed" --jobs=8 install
  INSTALL_COMMAND ""
  DEPENDS mbuild
  )


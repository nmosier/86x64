include(ExternalProject)

ExternalProject_Add(mbuild
  PREFIX mbuild
  GIT_REPOSITORY https://github.com/intelxed/mbuild
  GIT_TAG origin/master
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )

ExternalProject_Add(xed
  PREFIX xed
  GIT_REPOSITORY https://github.com/intelxed/xed
  GIT_TAG origin/master
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "./xed/mfile.py install"
  INSTALL_COMMAND ""
  DEPENDS mbuild
  )


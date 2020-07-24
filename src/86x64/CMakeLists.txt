add_library(wrapper STATIC wrapper.asm)
add_library(interpose SHARED interpose.c)

install(TARGETS wrapper interpose
  DESTINATION opt/86x64
  )

install(PROGRAMS 86x64 86x64.sh static-interpose.sh abiconv.sh
  DESTINATION opt/86x64
  )

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"${CMAKE_INSTALL_PREFIX}/opt/86x64/86x64.sh\" \"${CMAKE_INSTALL_PREFIX}/bin/86x64\")")

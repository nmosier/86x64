enable_language(ASM_NASM)

add_executable(abigen
  abigen.cc
  util.cc
  )

target_compile_options(abigen PRIVATE -pedantic -Wall)

add_library(wrapper STATIC wrapper.asm)
add_library(interpose SHARED interpose.c)

install(TARGETS wrapper interpose
  DESTINATION opt/86x64
  )

install(PROGRAMS 86x64 86x64.sh static-interpose.sh abiconv.sh
  DESTINATION opt/86x64
  )

install(TARGETS abigen
  DESTINATION opt/86x64
  )

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"${CMAKE_INSTALL_PREFIX}/opt/86x64/86x64.sh\" \"${CMAKE_INSTALL_PREFIX}/bin/86x64\")")

add_library(dyld_stub_binder STATIC dyld_stub_binder.asm)
install(TARGETS dyld_stub_binder DESTINATION opt/86x64)


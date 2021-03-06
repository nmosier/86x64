find_package(Clang REQUIRED PATHS /usr/local/opt/llvm)
include_directories(${CMAKE_SOURCE_DIR}/include/abiconv ${CLANG_INCLUDE_DIRS})

add_executable(abigen abigen.cc typeinfo.cc typeconv.cc loc.cc util.cc)
target_link_libraries(abigen libclang)
add_definitions(${LLVM_DEFINITIONS})
target_compile_options(abigen PRIVATE -Wno-format-security)

find_path(c_headers stdlib.h)
message(${c_headers})

add_custom_command(OUTPUT abiconv.syms
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/print_exports_trie.sh /usr/lib/system/libsystem_c.dylib /usr/lib/system/libsystem_kernel.dylib /usr/lib/system/libsystem_info.dylib /usr/lib/system/libsystem_malloc.dylib > abiconv.syms
  DEPENDS print_exports_trie.sh
  )

add_custom_target(abiconv_syms
  DEPENDS abiconv.syms
  )

add_custom_command(OUTPUT abiconv.asm
  COMMAND abigen -o abiconv.asm -s abiconv.syms -i ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms --structfile ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs ${CMAKE_CURRENT_SOURCE_DIR}/includes.h
  DEPENDS abigen abiconv.syms ${CMAKE_CURRENT_SOURCE_DIR}/custom.syms ${CMAKE_CURRENT_SOURCE_DIR}/ignore.structs ${CMAKE_CURRENT_SOURCE_DIR}/includes.h
  )

add_custom_target(abiconv_asm DEPENDS abiconv.asm)

# macro(create_abiconv TARGET_NAME INCLUDE_SYMS IGNORE_SYMS IGNORE_STRUCTS)
# endmacro()

macro(create_vararg_conv SYMBOL)
  add_library(vararg_conv_${SYMBOL} OBJECT vararg-conv-t.asm)
  target_compile_options(vararg_conv_${SYMBOL} PRIVATE "-DSYMBOL=${SYMBOL}")
endmacro()

create_vararg_conv(printf)
create_vararg_conv(sprintf)
create_vararg_conv(fprintf)
create_vararg_conv(snprintf)
create_vararg_conv(asprintf)
create_vararg_conv(dprintf)
create_vararg_conv(__sprintf_chk)

macro(create_custom_conv SOURCE)
  get_filename_component(SYMBOL ${SOURCE} NAME_WE)
  add_library(custom_conv_${SYMBOL} OBJECT ${SOURCE})
endmacro()

create_custom_conv(getopt.asm)
create_custom_conv(sigaction.asm)

macro(create_custom_conv_template SOURCE)
  foreach(SYMBOL ${ARGN})
    add_library(custom_conv_${SYMBOL} OBJECT ${SOURCE})
    target_compile_options(custom_conv_${SYMBOL} PRIVATE "-DSYMBOL=${SYMBOL}")
  endforeach()
endmacro()

create_custom_conv_template(exec.asm execvp)

add_library(abiconv SHARED
  abiconv.asm
  $<TARGET_OBJECTS:vararg_conv_printf>
  $<TARGET_OBJECTS:vararg_conv_sprintf>
  $<TARGET_OBJECTS:vararg_conv_fprintf>
  $<TARGET_OBJECTS:vararg_conv_snprintf>
  $<TARGET_OBJECTS:vararg_conv_asprintf>
  $<TARGET_OBJECTS:vararg_conv_dprintf>
  $<TARGET_OBJECTS:vararg_conv___sprintf_chk>
  $<TARGET_OBJECTS:custom_conv_getopt>
  $<TARGET_OBJECTS:custom_conv_sigaction>
  $<TARGET_OBJECTS:custom_conv_execvp>
  printf-conv.cc
  dyld_stub_binder.asm
  )
target_link_libraries(abiconv System)
add_dependencies(abiconv abiconv_asm)

install(TARGETS abiconv DESTINATION opt/86x64)

add_executable(info info.cc typeinfo.cc typeconv.cc loc.cc util.cc)
target_link_libraries(info libclang)
add_definitions(${LLVM_DEFINITIONS})


add_executable(convgen
  convgen.cc
  util.cc
  )
target_link_libraries(convgen libclang)
add_definitions(${LLVM_DEFINITIONS})

add_executable(syscall_tabgen
  syscall_tabgen.cc
  )


find_package(Clang REQUIRED PATHS /usr/local/opt/llvm)

add_executable(abigen abigen.cc)

target_link_libraries(abigen libclang)
target_include_directories(abigen PRIVATE ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
target_compile_options(abigen PRIVATE -Wno-format-security)

find_path(c_headers stdlib.h)
message(${c_headers})

add_custom_command(OUTPUT abiconv.syms
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/print_exports_trie.sh /usr/lib/system/libsystem_c.dylib /usr/lib/system/libsystem_kernel.dylib > abiconv.syms
  DEPENDS print_exports_trie.sh
  )

add_custom_target(abiconv_syms
  DEPENDS abiconv.syms
  )

add_custom_command(OUTPUT abiconv.asm
  COMMAND abigen -o abiconv.asm ${c_headers}/dirent.h ${c_headers}/stdlib.h ${c_headers}/stdio.h ${c_headers}/unistd.h ${c_headers}/errno.h < abiconv.syms
  DEPENDS abigen abiconv_syms
  )

add_custom_target(abiconv_asm DEPENDS abiconv.asm)

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

add_library(abiconv SHARED
  abiconv.asm
  $<TARGET_OBJECTS:vararg_conv_printf>
  $<TARGET_OBJECTS:vararg_conv_sprintf>
  $<TARGET_OBJECTS:vararg_conv_fprintf>
  $<TARGET_OBJECTS:vararg_conv_snprintf>
  $<TARGET_OBJECTS:vararg_conv_asprintf>
  $<TARGET_OBJECTS:vararg_conv_dprintf>
  conv.cc
  dyld_stub_binder.asm
  )
target_link_libraries(abiconv System)
add_dependencies(abiconv abiconv_asm)

install(TARGETS abiconv DESTINATION opt/86x64)

add_executable(info info.cc)
target_link_libraries(info libclang)
target_include_directories(info PRIVATE ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

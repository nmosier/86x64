find_package(Clang REQUIRED PATHS /usr/local/opt/llvm)

add_executable(abigen abigen.cc)

target_link_libraries(abigen libclang)
target_include_directories(abigen PRIVATE ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_custom_command(OUTPUT abiconv.asm
  COMMAND abigen -o abiconv.asm /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/dirent.h
  DEPENDS abigen # symdefs.h
  )

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

install(TARGETS abiconv DESTINATION opt/86x64)

add_executable(info info.cc)
target_link_libraries(info libclang)
target_include_directories(info PRIVATE ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

function(add_86x64 tgt_name 86x64_NAME)
  cmake_parse_arguments(
    86x64
    ""
    "OUTPUT;LIBABICONV;WRAPPER;LIBINTERPOSE;ARCHIVE64;MACHO_TOOL;ARCHIVE32;DYLIB;STATIC_INTERPOSE;INTERPOSE_SH;SOURCE;OPTIM"
    ""
    ${ARGN}
    )
  
  set(86x64_NAME32 ${86x64_NAME}-32)
  set(86x64_NAME64 ${86x64_NAME}-64)

  if (NOT DEFINED 86x64_OPTIM)
    set(86x64_OPTIM O0)
  endif()

  if (NOT DEFINED 86x64_SOURCE)
    set(86x64_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${86x64_NAME}.c)
    if (NOT EXISTS ${86x64_SOURCE})
      message(FATAL_ERROR "no SOURCE parameter set and default source ${86x64_SOURCE} doesn't exist")
    endif()
  endif()

  if (NOT DEFINED 86x64_ARCHIVE32)
    set(86x64_ARCHIVE32 ${86x64_NAME32})
  endif()
  set(86x64_OBJ32 ${86x64_NAME32}.o)
  
  if (NOT DEFINED 86x64_ARCHIVE64)
    set(86x64_ARCHIVE64 ${86x64_NAME64})
  endif()

  if (NOT DEFINED 86x64_DYLIB)
    set(86x64_DYLIB "${86x64_ARCHIVE64}.dylib")
  endif()

  if(NOT DEFINED 86x64_STATIC_INTERPOSE)
    message(FATAL_ERROR "missing STATIC_INTERPOSE argument")
  elseif(NOT EXISTS ${86x64_STATIC_INTERPOSE})
    message(FATAL_ERROR "static interpose doesn't exist")
  endif()

  if(NOT DEFINED 86x64_INTERPOSE_SH)
    set(86x64_INTERPOSE_SH ${CMAKE_SOURCE_DIR}/src/86x64/interpose.sh)
  endif()
  if(NOT EXISTS ${86x64_INTERPOSE_SH})
    message(FATAL_ERROR "interpose script not found")
  endif()

  set(86x64_REBASE ${86x64_NAME32}_rebase)
  set(86x64_TRANSFORM ${86x64_NAME64}_transform)
  set(86x64_ABICONV ${86x64_NAME64}_abiconv)
  set(86x64_DOLLAR ${86x64_NAME64}_dollar)
  set(86x64_LAZY_BIND_SYMS ${86x64_NAME64}_lazy_bind.syms)
  set(86x64_INTERPOSE ${86x64_NAME64}_interpose)
  set(86x64_DYLD ${86x64_NAME64}_dyld)

  add_custom_command(OUTPUT ${86x64_OBJ32}
    COMMAND cc -${OPTIM} -fno-stack-protector -fno-stack-check -c -m32 -o ${86x64_OBJ32} ${86x64_SOURCE}
    DEPENDS ${86x64_SOURCE}
    )

  add_custom_command(OUTPUT ${86x64_ARCHIVE32}
    COMMAND ld -lsystem -o ${86x64_ARCHIVE32} ${86x64_OBJ32}
    DEPENDS ${86x64_OBJ32}
    )

  # rebasify 32-bit archive
  add_custom_command(OUTPUT ${86x64_REBASE}
    COMMAND macho-tool rebasify ${86x64_ARCHIVE32} ${86x64_REBASE}
    DEPENDS macho-tool ${86x64_ARCHIVE32}
    )

  # transform 32-bit archive to 64-bit archive
  add_custom_command(OUTPUT ${86x64_TRANSFORM}
    COMMAND macho-tool transform ${86x64_REBASE} ${86x64_TRANSFORM}
    DEPENDS macho-tool ${86x64_REBASE}
    )

  # link 64-bit archive with libabiconv
  add_custom_command(OUTPUT ${86x64_ABICONV}
    COMMAND macho-tool modify --insert load-dylib,name=$<TARGET_FILE:abiconv> ${86x64_TRANSFORM} ${86x64_ABICONV}
    DEPENDS macho-tool abiconv ${86x64_TRANSFORM}
    )

  # strip appropriate versioning suffixes 
  add_custom_command(OUTPUT ${86x64_DOLLAR}
    COMMAND macho-tool modify --update strip-bind,suffix='$$UNIX2003' ${86x64_ABICONV} ${86x64_DOLLAR}
    DEPENDS macho-tool ${86x64_ABICONV}
    )

  # gather lazily bound symbols
  add_custom_command(OUTPUT ${86x64_LAZY_BIND_SYMS}
    COMMAND macho-tool print --lazy-bind ${86x64_DOLLAR} | tail +2 | cut -d " " -f 5 > ${86x64_LAZY_BIND_SYMS}
    DEPENDS macho-tool ${86x64_DOLLAR}
    )

  # statically interpose lazily bound symbols
  add_custom_command(OUTPUT ${86x64_INTERPOSE}
    COMMAND ${86x64_STATIC_INTERPOSE} -l $<TARGET_FILE:abiconv> -p "__" -o ${86x64_INTERPOSE} ${86x64_DOLLAR} < ${86x64_LAZY_BIND_SYMS}
    DEPENDS macho-tool ${86x64_DOLLAR} ${86x64_LAZY_BIND_SYMS}
    )

  # interpose dyld_stub_binder
  add_custom_command(OUTPUT ${86x64_DYLD}
    COMMAND ${86x64_INTERPOSE_SH} -m $<TARGET_FILE:macho-tool> -l $<TARGET_FILE:abiconv> -o ${86x64_DYLD} ${86x64_INTERPOSE}
    DEPENDS ${86x64_INTERPOSE_SH} macho-tool abiconv ${86x64_INTERPOSE}
    )

  # convert result to dylib
  add_custom_command(OUTPUT ${86x64_DYLIB}
    COMMAND macho-tool convert --archive DYLIB ${86x64_DYLD} ${86x64_DYLIB}
    DEPENDS macho-tool ${86x64_DYLD}
    )

  # link wrapper
  set(86x64_INTERPOSE_RPATH "${CMAKE_BINARY_DIR}/src/86x64")
  set(86x64_ABICONV_RPATH "${CMAKE_BINARY_DIR}/src/abiconv")
  # get_filename_component(86x64_INTERPOSE_RPATH $<TARGET_FILE:interpose> DIRECTORY)
  # get_filename_component(86x64_ABICONV_RPATH $<TARGET_FILE:abiconv> DIRECTORY)
  add_custom_command(OUTPUT ${86x64_ARCHIVE64}
    COMMAND ld -arch x86_64 -rpath ${86x64_INTERPOSE_RPATH} -rpath ${86x64_ABICONV_RPATH} -pagezero_size 0x1000 -lsystem -e _main_wrapper -o ${86x64_ARCHIVE64} $<TARGET_FILE:wrapper> $<TARGET_FILE:interpose> ${86x64_DYLIB}
    DEPENDS wrapper interpose ${86x64_DYLIB}
    )

  if(DEFINED ${tgt_name})
    message(FATAL_ERROR "target name '${tgt_name}' already in use")
  endif()
  
  add_custom_target(${tgt_name} ALL
    DEPENDS ${86x64_DYLIB} ${86x64_ARCHIVE64}
    )
  
endfunction()

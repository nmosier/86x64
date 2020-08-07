function(add_86x64 86x64_NAME)
  cmake_parse_arguments(
    86x64
    ""
    "OUTPUT;LIBABICONV;WRAPPER;LIBINTERPOSE;ARCHIVE64;MACHO_TOOL;ARCHIVE32;DYLIB;STATIC_INTERPOSE;INTERPOSE_SH;SOURCE"
    ""
    ${ARGN}
    )
  
  set(86x64_NAME32 ${86x64_NAME}32)
  set(86x64_NAME64 ${86x64_NAME}64)

  if (NOT DEFINED 86x64_SOURCE)
    set(86x64_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${86x64_NAME}.c)
    if (NOT EXISTS ${86x64_SOURCE})
      message(FATAL_ERROR "no SOURCE parameter set and default source (${86x64_SOURCE}) doesn't exist")
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

  if (NOT DEFINED 86x64_MACHO_TOOL)
    if (TARGET macho-tool)
      set(86x64_MACHO_TOOL $<TARGET_FILE:macho-tool>)
    else()
      message(FATAL_ERROR "86x64_MACHO_TOOL parameter not provided and macho_tool target not found")
    endif()
  endif()

  if (NOT DEFINED 86x64_LIBABICONV)
    if (TARGET abiconv) 
      set(86x64_LIBABICONV $<TARGET_FILE:abiconv>)
    else()
      message(FATAL_ERROR "86x64_LIBABICONV parameter not provided and abiconv target not found")
    endif()
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

  if(NOT DEFINED 86x64_WRAPPER)
    if (TARGET wrapper)
      set(86x64_WRAPPER $<TARGET_FILE:wrapper>)
    else()
      message(FATAL_ERROR "could not locate wrapper library")
    endif()
  endif()

  if(NOT DEFINED 86x64_LIBINTERPOSE)
    if(TARGET interpose) 
      set(86x64_LIBINTERPOSE $<TARGET_FILE:interpose>)
    else()
      message(FATAL_ERROR "libinterpose not found")
    endif()
  endif()

  set(86x64_REBASE ${86x64_NAME32}_rebase)
  set(86x64_TRANSFORM ${86x64_NAME64}_transform)
  set(86x64_ABICONV ${86x64_NAME64}_abiconv)
  set(86x64_DOLLAR ${86x64_NAME64}_dollar)
  set(86x64_LAZY_BIND_SYMS ${86x64_NAME64}_lazy_bind.syms)
  set(86x64_INTERPOSE ${86x64_NAME64}_interpose)
  set(86x64_DYLD ${86x64_NAME64}_dyld)

  add_custom_command(OUTPUT ${86x64_OBJ32}
    COMMAND cc -fno-stack-protector -fno-stack-check -c -m32 -o ${86x64_OBJ32} ${86x64_SOURCE}
    DEPENDS ${86x64_SOURCE}
    )

  add_custom_command(OUTPUT ${86x64_ARCHIVE32}
    COMMAND ld -lsystem -o ${86x64_ARCHIVE32} ${86x64_OBJ32}
    DEPENDS ${86x64_OBJ32}
    )

  # rebasify 32-bit archive
  add_custom_command(OUTPUT ${86x64_REBASE}
    COMMAND ${86x64_MACHO_TOOL} rebasify ${86x64_ARCHIVE32} ${86x64_REBASE}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_ARCHIVE32}
    )

  # transform 32-bit archive to 64-bit archive
  add_custom_command(OUTPUT ${86x64_TRANSFORM}
    COMMAND ${86x64_MACHO_TOOL} transform ${86x64_REBASE} ${86x64_TRANSFORM}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_REBASE}
    )

  # link 64-bit archive with libabiconv
  add_custom_command(OUTPUT ${86x64_ABICONV}
    COMMAND ${86x64_MACHO_TOOL} modify --insert load-dylib,name=${86x64_LIBABICONV} ${86x64_TRANSFORM} ${86x64_ABICONV}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_LIBABICONV} ${86x64_TRANSFORM}
    )

  # strip appropriate versioning suffixes 
  add_custom_command(OUTPUT ${86x64_DOLLAR}
    COMMAND ${86x64_MACHO_TOOL} modify --update strip-bind,suffix="$UNIX2003" ${86x64_ABICONV} ${86x64_DOLLAR}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_ABICONV}
    )

  # gather lazily bound symbols
  add_custom_command(OUTPUT ${86x64_LAZY_BIND_SYMS}
    COMMAND ${86x64_MACHO_TOOL} print --lazy-bind ${86x64_DOLLAR} | tail +2 | cut -d " " -f 5 > ${86x64_LAZY_BIND_SYMS}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_DOLLAR}
    )

  # statically interpose lazily bound symbols
  add_custom_command(OUTPUT ${86x64_INTERPOSE}
    COMMAND ${86x64_STATIC_INTERPOSE} -l ${86x64_LIBABICONV} -p "__" -o ${86x64_INTERPOSE} ${86x64_DOLLAR} < ${86x64_LAZY_BIND_SYMS}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_DOLLAR} ${86x64_LAZY_BIND_SYMS}
    )

  # interpose dyld_stub_binder
  add_custom_command(OUTPUT ${86x64_DYLD}
    COMMAND ${86x64_INTERPOSE_SH} -m ${86x64_MACHO_TOOL} -l ${86x64_LIBABICONV} -o ${86x64_DYLD} ${86x64_INTERPOSE}
    DEPENDS ${86x64_INTERPOSE_SH} ${86x64_MACHO_TOOL} ${86x64_LIBABICONV} ${86x64_INTERPOSE}
    )

  # convert result to dylib
  add_custom_command(OUTPUT ${86x64_DYLIB}
    COMMAND ${86x64_MACHO_TOOL} convert --archive DYLIB ${86x64_DYLD} ${86x64_DYLIB}
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_DYLD}
    )

  # link wrapper
  get_filename_component(86x64_INTERPOSE_RPATH ${86x64_LIBINTERPOSE} DIRECTORY)
  get_filename_component(86x64_ABICONV_RPATH ${86x64_LIBABICONV} DIRECTORY)
  add_custom_command(OUTPUT ${86x64_ARCHIVE64}
    COMMAND ld -arch x86_64 -rpath ${86x64_INTERPOSE_RPATH} -rpath ${86x64_ABICONV_RPATH} -pagezero_size 0x1000 -lsystem -e _main_wrapper -o ${86x64_ARCHIVE64} ${86x64_WRAPPER} ${86x64_LIBINTERPOSE} ${86x64_DYLIB}
    DEPENDS ${86x64_WRAPPER} ${86x64_LIBINTERPOSE} ${86x64_DYLIB}
    )
  
  add_custom_target(${86x64_NAME64} DEPENDS ${86x64_DYLIB})
  
endfunction()

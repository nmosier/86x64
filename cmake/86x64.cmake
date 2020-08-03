function(add_86x86 NAME)
  cmake_parse_arguments(
    86x64
    ""
    "OUTPUT;LIBABICONV;WRAPPER;LIBINTERPOSE;ARCHIVE64;MACHO_TOOL;ARCHIVE32;DYLIB;STATIC_INTERPOSE"
    ""
    ${ARGN}
    )

  if (NOT_DEFINED 86x64_ARCHIVE32)
    set(86x64_ARCHIVE32 ${NAME}32)
  endif()

  if (NOT DEFINED 86x64_ARCHIVE64)
    set(86x64_ARCHIVE64 ${NAME}64)
  endif()
  
  if (NOT DEFINED 86x64_DYLIB)
    set(86x64_DYLIB "${86x64_ARCHIVE64}.dylib")
  endif()

  if (NOT DEFINED 86x64_MACHO_TOOL)
    set(86x64_MACHO_TOOL $<TARGET_FILE:macho_tool>)
  endif()

  if (NOT DEFINED 86x64_LIBABICONV)
    set(86x64_LIBABICONV $<TARGET_FILE:abiconv>)
  endif()

  # rebasify 32-bit archive
  add_custom_command(OUTPUT ${NAME}_rebasify
    COMMAND ${86x64_MACHO_TOOL} rebasify ${86x64_ARCHIVE32} ${NAME}_REBASIFY
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_ARCHIVE32}
    )

  # transform 32-bit archive to 64-bit archive
  add_custom_command(OUTPUT ${NAME}_transform
    COMMAND ${86x64_MACHO_TOOL} transform ${NAME}_rebasify ${NAME}_transform
    DEPENDS ${86x64_MACHO_TOOL} ${NAME}_rebasify
    )

  # link 64-bit archive with libabiconv
  add_custom_command(OUTPUT ${NAME}_abiconv
    COMMAND ${86x64_MACHO_TOOL} modify --insert load-dylib,name=${86x64_LIBABICONV} ${NAME}_transform ${NAME}_abiconv
    DEPENDS ${86x64_MACHO_TOOL} ${86x64_LIBABICONV} ${NAME}_transform
    )

  # strip appropriate versioning suffixes 
  add_custom_command(OUTPUT ${NAME}_dollar
    COMMAND ${86x64_MACHO_TOOL} modify --update strip-bind,suffix="$UNIX2003" ${NAME}_abiconv ${NAME}_dollar
    DEPENDS ${86x64_MACHO_TOOL} ${NAME}_abiconv
    )

  # gather lazily bound symbols
  add_custom_command(OUTPUT ${NAME}_lazy_bind.syms
    COMMAND ${86x64_MACHO_TOOL} print --lazy-bind ${NAME}_dollar | tail +2 | cut -d " " -f 5 > ${NAME}_lazy_bind.syms
    DEPENDS ${86x64_MACHO_TOOL} ${NAME}_dollar
    )

  # statically interpose lazily bound symbols
  add_custom_command(OUTPUT ${NAME}_interpose
    COMMAND xargs ${86x64_STATIC_INTERPOSE} -l ${86x64_LIBABICONV} -p "__" -o ${name}_interpose ${name}_dollar < ${NAME}_lazy_bind.syms
    DEPENDS ${86x64_MACHO_TOOL} ${NAME}_interpose ${NAME}_dollar ${NAME}_lazy_bind.syms
    )

  # interpose dyld_stub_binder
  add_custom_command(OUTPUT ${NAME}_
  
endfunction()

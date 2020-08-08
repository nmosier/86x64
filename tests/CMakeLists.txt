function(create_test)
  foreach(TEST ${ARGN})
    foreach(OPTIM O0 O1 O2 O3)
      set(TESTNAME ${TEST}-${OPTIM})
      add_86x64(${TESTNAME} ${TESTNAME}
        OPTIM ${OPTIM}
        SOURCE ${TEST}.c
        STATIC_INTERPOSE ${CMAKE_SOURCE_DIR}/src/86x64/static-interpose.sh
        )
    endforeach()
    add_test(NAME ${TEST}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/do-test.sh "${CMAKE_CURRENT_BINARY_DIR}/${TEST}"
      )
  endforeach()
endfunction()

create_test(file exit printf-many printf sum sprintf fprintf myls in_addr mysh)

# add_86x64(tmp tmp
#   STATIC_INTERPOSE ${CMAKE_SOURCE_DIR}/src/86x64/static-interpose.sh
#   )

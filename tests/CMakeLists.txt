function(create_test)
  foreach(TEST ${ARGN})
    foreach(OPTIM O0 O1 O2 O3)
      set(TESTNAME ${TEST}-${OPTIM})
      set(TESTNAME32 ${TESTNAME}-32)
      set(TESTNAME64 ${TESTNAME}-64)
      set(TESTSRC ${TEST}.c)
      
      add_custom_command(OUTPUT ${TESTNAME32}
        COMMAND cc -${OPTIM} -fno-stack-protector -fno-stack-check -c -m32 -o ${TESTNAME32}.c.o ${CMAKE_CURRENT_SOURCE_DIR}/${TESTSRC}
        COMMAND ld -lsystem -o ${TESTNAME32} ${TESTNAME32}.c.o
        BYPRODUCTS ${TESTNAME32}.c.o
        DEPENDS ${TESTSRC}
        )
      
      add_custom_target(${TESTNAME32}_tgt ALL
        DEPENDS ${TESTNAME32}
        )

      add_custom_command(OUTPUT ${TESTNAME64} ${TESTNAME64}.dylib
        COMMAND ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh -o ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME64} -l $<TARGET_FILE:abiconv> -i $<TARGET_FILE:interpose> -a ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME64}.dylib -m $<TARGET_FILE:macho-tool> -w $<TARGET_FILE:wrapper> ${TESTNAME32}
        DEPENDS ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh ${TESTNAME32} abiconv interpose macho-tool
        BYPRODUCTS ${TESTNAME32}_rebase ${TESTNAME64}_transform ${TESTNAME64}_abi ${TESTNAME64}_dollar ${TESTNAME64}_interpose ${TESTNAME64}_dyld
        )
      
      add_custom_target(${TESTNAME64}_tgt ALL
        DEPENDS ${TESTNAME64} ${TESTNAME64}.dylib
        )
      
    endforeach()

    add_test(NAME ${TEST}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/do-test.sh "${CMAKE_CURRENT_BINARY_DIR}/${TEST}"
      )
    
  endforeach()
  
endfunction()

create_test(file exit printf-many printf sum sprintf fprintf myls in_addr mysh)


add_86x64(tmp tmp
  STATIC_INTERPOSE ${CMAKE_SOURCE_DIR}/src/86x64/static-interpose.sh
  )

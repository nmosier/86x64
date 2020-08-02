macro(create_test)
  foreach(TESTNAME ${ARGN})
    add_custom_command(OUTPUT ${TESTNAME}32
      COMMAND cc -O3 -fno-stack-protector -fno-stack-check -c -m32 -o ${TESTNAME}32.c.o ${CMAKE_CURRENT_SOURCE_DIR}/${TESTNAME}.c
      COMMAND ld -lsystem -o ${TESTNAME}32 ${TESTNAME}32.c.o
      BYPRODUCTS ${TESTNAME}32.c.o
      DEPENDS ${TESTNAME}.c
      )
    
    add_custom_target(${TESTNAME}32_tgt ALL
      DEPENDS ${TESTNAME}32
      )
    
    add_custom_command(OUTPUT ${TESTNAME}64 ${TESTNAME}64.dylib
      COMMAND ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh -o ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}64 -l $<TARGET_FILE:abiconv> -i $<TARGET_FILE:interpose> -a ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}64.dylib -m $<TARGET_FILE:macho-tool> -w $<TARGET_FILE:wrapper> ${TESTNAME}32
      DEPENDS ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh ${TESTNAME}32 abiconv interpose macho-tool
      )
    
    add_custom_target(${TESTNAME}64_tgt ALL
      DEPENDS ${TESTNAME}64 ${TESTNAME}64.dylib
      )
    
    add_test(NAME ${TESTNAME}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/do-test.sh ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}
      )
  endforeach()
  
endmacro()

create_test(file exit printf-many printf sum sprintf fprintf myls in_addr mysh)

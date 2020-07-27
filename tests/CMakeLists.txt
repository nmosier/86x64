macro(create_test TESTNAME)
  add_custom_command(OUTPUT ${TESTNAME}32
    COMMAND cc -fno-stack-protector -fno-stack-check -c -m32 -o ${TESTNAME}32.c.o ${TESTNAME}.c
    COMMAND ld -lsystem -o ${TESTNAME}32 ${TESTNAME}32.c.o
    BYPRODUCTS ${TESTNAME}32.c.o
    DEPENDS ${TESTNAME}.c
    )

  add_custom_target(${TESTNAME}32_tgt ALL
    DEPENDS ${TESTNAME}32
    )

  add_custom_command(OUTPUT ${TESTNAME}64 ${TESTNAME}64.dylib
    COMMAND ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh -o ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}64 -l $<TARGET_FILE:abiconv> -i $<TARGET_FILE:interpose> -a ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}64.dylib -m $<TARGET_FILE:macho-tool> ${TESTNAME}32
    DEPENDS ${PROJECT_SOURCE_DIR}/src/86x64/86x64.sh ${TESTNAME}32 abiconv interpose macho-tool
    )

  add_custom_target(${TESTNAME}64_tgt ALL
    DEPENDS ${TESTNAME}64 ${TESTNAME}64.dylib
    )

  add_test(NAME ${TESTNAME}
    COMMAND do-test.sh ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME}
    )
  
endmacro()

create_test(file)
create_test(exit)
create_test(printf-many)
create_test(printf)
create_test(sum)
create_test(sprintf)
create_test(fprintf)
create_test(myls)


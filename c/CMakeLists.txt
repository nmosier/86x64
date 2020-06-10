add_executable(macho
  main.c
  util.c
  macho-parse.c
  macho-util.c
  macho-emit.c
  macho-patch.c
  macho-build.c
  macho-sizeof.c
  macho-transform.c
  )

target_link_libraries(macho PRIVATE ${LIEF_LIBRARIES} ${xed_LIBRARIES})
target_include_directories(macho PRIVATE ${LIEF_INCLUDE_DIRS} ${xed_INCLUDE_DIRS})
target_compile_options(macho PRIVATE -pedantic -Wall)

add_executable(uleb-test uleb-test.c util.c)
add_executable(sleb-test sleb-test.c util.c)

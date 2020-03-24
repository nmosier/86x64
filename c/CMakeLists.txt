add_executable(macho main.c util.c macho-parse.c macho-util.c macho-emit.c
  macho-build.c macho-sizeof.c)
target_link_libraries(macho PRIVATE ${LIEF_LIBRARIES})
target_include_directories(macho PRIVATE ${LIEF_INCLUDE_DIRS})

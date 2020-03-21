add_executable(macho main.c transform.c util.c macho.c)
target_link_libraries(macho PRIVATE ${LIEF_LIBRARIES})
target_include_directories(macho PRIVATE ${LIEF_INCLUDE_DIRS})

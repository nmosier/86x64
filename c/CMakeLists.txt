add_library(macho_objs
  OBJECT
  util.c
  macho-parse.c
  macho-util.c
  macho-emit.c
  macho-patch.c
  macho-build.c
  macho-sizeof.c
  macho-transform.c
  macho-insert.c
  macho-driver.c
  )

target_include_directories(macho_objs PRIVATE ${xed_INCLUDE_DIRS})
add_compile_options(-pedantic -Wall)

add_executable(macho-transform
  macho-transform-main.c
  $<TARGET_OBJECTS:macho_objs>
)
target_link_libraries(macho-transform PRIVATE ${xed_LIBRARIES})

# target_include_directories(macho PRIVATE ${LIEF_INCLUDE_DIRS} ${xed_INCLUDE_DIRS})
# target_compile_options(macho PRIVATE -pedantic -Wall)

add_executable(uleb-test uleb-test.c util.c)
add_executable(sleb-test sleb-test.c util.c)

add_executable(driver driver.c)
target_link_libraries(driver PRIVATE ${xed_LIBRARIES})
target_include_directories(driver PRIVATE ${xed_INCLUDE_DIRS})

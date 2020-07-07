link_directories($ENV{HOME}/lib)
include_directories($ENV{HOME}/include)

add_library(macho_cxx_objs
  OBJECT
  segment.cc
  linkedit.cc
  lc.cc
  dyldinfo.cc
  util.cc
  image.cc
  parse.cc
  leb.c
  build.cc
  symtab.cc
  loc.cc
  section.cc
  archive.cc
  macho.cc
  region.cc
  data_in_code.cc
  section_blob.cc
  instruction.cc
  fat.cc
  tweak.cc
  )

target_link_libraries(macho_cxx_objs PRIVATE ${xed_LIBRARIES})
target_include_directories(macho_cxx_objs PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho_cxx_objs PRIVATE -pedantic -Wall -Wno-format-security)

add_subdirectory(macho-tool)

add_executable(macho+noop
  macho-noop-main.cc
  $<TARGET_OBJECTS:macho_cxx_objs>
  )

add_executable(macho+insert+instruction
  macho-insert-instruction-main.cc
  $<TARGET_OBJECTS:macho_cxx_objs>
  )

target_link_libraries(macho+noop PRIVATE ${xed_LIBRARIES} scanopt)
target_include_directories(macho+noop PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho+noop PRIVATE -pedantic -Wall -Wno-format-security)

target_link_libraries(macho+insert+instruction PRIVATE ${xed_LIBRARIES} scanopt)
target_include_directories(macho+insert+instruction PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho+insert+instruction PRIVATE -pedantic -Wall -Wno-format-security)


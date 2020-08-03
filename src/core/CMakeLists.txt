# link_directories($ENV{HOME}/lib)
# include_directories($ENV{HOME}/include)
include_directories("${PROJECT_SOURCE_DIR}/include/core")

add_library(core_objs
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
  rebase_info.cc
  export_info.cc
  opcodes.cc
  transform.cc
  stub_helper.cc
  resolve.cc
  )
add_dependencies(core_objs xed)

target_link_libraries(core_objs PRIVATE ${xed_LIBRARIES})
target_include_directories(core_objs PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(core_objs PRIVATE -pedantic -Wall -Wno-format-security)


add_executable(test-encode
  test-encode.cc
  )
add_dependencies(test-encode xed)
target_link_libraries(test-encode PRIVATE ${xed_LIBRARIES})
target_include_directories(test-encode PRIVATE ${xed_INCLUDE_DIRS})

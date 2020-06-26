add_executable(macho
  macho.cc
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
)

target_link_libraries(macho PRIVATE ${xed_LIBRARIES})
target_include_directories(macho PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho PRIVATE -pedantic -Wall -Wno-format-security)

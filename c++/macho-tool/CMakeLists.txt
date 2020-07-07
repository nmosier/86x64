include_directories("..")

add_executable(macho-tool
  macho-tool.cc
  command.cc
  util.cc
  noop.cc
  modify.cc
  translate.cc
  tweak.cc
  convert.cc
  operation.cc
  $<TARGET_OBJECTS:macho_cxx_objs>
  )

target_link_libraries(macho-tool PRIVATE ${xed_LIBRARIES} scanopt)
target_include_directories(macho-tool PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho-tool PRIVATE -pedantic -Wall -Wno-format-security
  -Wno-writable-strings)

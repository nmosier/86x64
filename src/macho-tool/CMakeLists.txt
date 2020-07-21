include_directories("${PROJECT_SOURCE_DIR}/include/macho-tool")
include_directories("$ENV{HOME}/include")
link_directories("$ENV{HOME}/lib")

add_executable(macho-tool
  macho-tool.cc
  command.cc
  util.cc
  noop.cc
  modify.cc
  translate.cc
  tweak.cc
  convert.cc
  modify-insert.cc
  modify-delete.cc
  modify-start.cc
  modify-update.cc
  transform.cc
  print.cc
  rebasify.cc
  $<TARGET_OBJECTS:core_objs>
  )

target_link_libraries(macho-tool PRIVATE ${xed_LIBRARIES} scanopt)
target_include_directories(macho-tool PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho-tool PRIVATE -pedantic -Wall -Wno-format-security -Wno-writable-strings)

install(TARGETS macho-tool
  RUNTIME
  DESTINATION bin
  )

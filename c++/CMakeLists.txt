add_executable(macho
  macho.cc
  section.cc
  linkedit.cc
  util.cc
)

target_link_libraries(macho PRIVATE ${xed_LIBRARIES})
target_include_directories(macho PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho PRIVATE -pedantic -Wall -Wno-format-security)

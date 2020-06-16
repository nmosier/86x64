add_executable(macho
  macho.cc
  util.cc
)

target_link_libraries(macho PRIVATE ${xed_LIBRARIES})
target_include_directories(macho PRIVATE ${xed_INCLUDE_DIRS})
target_compile_options(macho PRIVATE -pedantic -Wall -Wno-format-security)

add_executable(leb
  leb-main.cc
  leb.c
)

target_include_directories(leb PRIVATE $ENV{HOME}/include)
target_link_directories(leb PRIVATE $ENV{HOME}/lib)
target_link_libraries(leb PRIVATE scanopt)

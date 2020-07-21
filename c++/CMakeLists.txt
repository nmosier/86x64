link_directories($ENV{HOME}/lib)
include_directories($ENV{HOME}/include)

add_subdirectory(macho-tool)

# add_executable(trie-test trie-test.cc )

add_subdirectory(abiconv)

install(TARGETS macho-tool
  RUNTIME
  DESTINATION bin
  )

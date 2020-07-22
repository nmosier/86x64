add_executable(abigen
  abigen.cc
  util.cc
  )

target_compile_options(abigen PRIVATE -pedantic -Wall)

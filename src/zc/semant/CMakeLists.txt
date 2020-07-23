BISON_TARGET(zc_parser c.ypp ${CMAKE_CURRENT_BINARY_DIR}/zc-parser.cpp)
FLEX_TARGET(zc_lexer c.l ${CMAKE_CURRENT_BINARY_DIR}/zc-lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(zc_lexer zc_parser)
add_compile_definitions(ZC_PARSER_HEADER=\"${BISON_zc_parser_OUTPUT_HEADER}\")

set_source_files_properties(
  ${FLEX_zc_lexer_OUTPUTS}
  PROPERTIES
  COMPILE_FLAGS -Wno-register
)

add_library(semant_objs
  OBJECT
  semant.cpp
  ${FLEX_zc_lexer_OUTPUTS}
  ${BISON_zc_parser_OUTPUTS}
)

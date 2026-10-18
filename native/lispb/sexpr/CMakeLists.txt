add_library(lispb-strings OBJECT)
target_sources(lispb-strings
  PRIVATE
    ../src/strings.cpp
)
target_include_directories(lispb-strings
  PUBLIC
    ../include
)
target_link_libraries(lispb-strings
  PUBLIC
    sandbox::native_defaults
)
ioj_enable_warnings(lispb-strings CODEGEN)
ioj_apply_native_abi(lispb-strings)

add_library(lispb-sexpr-output OBJECT)
target_sources(lispb-sexpr-output
  PRIVATE
    src/output/form_builder.cpp
    src/output/sexpr_ir.cpp
)
target_include_directories(lispb-sexpr-output
  PUBLIC
    include
    ../include
)
target_link_libraries(lispb-sexpr-output
  PUBLIC
    sandbox::native_defaults
)
ioj_enable_warnings(lispb-sexpr-output CODEGEN)
ioj_apply_native_abi(lispb-sexpr-output)

add_library(lispb-syntax)
target_sources(lispb-syntax
  PRIVATE
    src/fields.cpp
    src/parser.cpp
    ../src/diagnostics.cpp
    src/lexer.cpp
    src/reader.cpp
    src/syntax_parser.cpp
    src/syntax.cpp
    src/sources.cpp
    src/source_location.cpp
    src/source_error.cpp
)
target_include_directories(lispb-syntax
  PUBLIC
    include
    ../include
)
target_link_libraries(lispb-syntax
  PUBLIC
    sandbox::native_defaults
  PRIVATE
    lispb-strings
)
ioj_enable_warnings(lispb-syntax CODEGEN)

add_executable(lispb-syntax-tests tests/reader_tests.cpp)
target_link_libraries(lispb-syntax-tests PRIVATE
  lispb-syntax
  GTest::gtest_main
  sandbox::native_defaults
)
ioj_enable_warnings(lispb-syntax-tests CODEGEN)
add_test(NAME LispbSyntaxTests COMMAND lispb-syntax-tests)
set_tests_properties(LispbSyntaxTests PROPERTIES LABELS "unit;all;native;codegen")

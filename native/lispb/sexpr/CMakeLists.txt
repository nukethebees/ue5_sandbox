add_library(lispb-sexpr-output INTERFACE)
target_include_directories(lispb-sexpr-output
  INTERFACE
    include
    ../include
)

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
    ../src/strings.cpp
)
target_include_directories(lispb-syntax
  PUBLIC
    include
    ../include
)
target_link_libraries(lispb-syntax
  PUBLIC
    sandbox::native_defaults
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

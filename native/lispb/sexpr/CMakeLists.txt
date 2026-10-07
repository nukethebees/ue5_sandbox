add_library(lispb-syntax
  src/fields.cpp
  src/lexer.cpp
  src/reader.cpp
)
target_include_directories(lispb-syntax PUBLIC include)
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

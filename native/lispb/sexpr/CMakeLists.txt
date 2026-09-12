add_library(lispb-syntax
  src/lexer.cpp
  src/reader.cpp
)
target_compile_features(lispb-syntax PUBLIC cxx_std_23)
target_include_directories(lispb-syntax PUBLIC include)
target_link_libraries(lispb-syntax PRIVATE sandbox::codegen_warnings)

add_executable(lispb-syntax-tests tests/reader_tests.cpp)
target_link_libraries(lispb-syntax-tests PRIVATE
  lispb-syntax
  GTest::gtest_main
  sandbox::codegen_warnings
)
add_test(NAME LispbSyntaxTests COMMAND lispb-syntax-tests)
set_tests_properties(LispbSyntaxTests PROPERTIES LABELS "unit;all;codegen")

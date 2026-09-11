add_library(codegen-sexpr
  src/lexer.cpp
  src/reader.cpp
)
target_compile_features(codegen-sexpr PUBLIC cxx_std_23)
target_include_directories(codegen-sexpr PUBLIC include)
target_link_libraries(codegen-sexpr PRIVATE sandbox::codegen_warnings)

add_executable(codegen-sexpr-tests tests/reader_tests.cpp)
target_link_libraries(codegen-sexpr-tests PRIVATE
  codegen-sexpr
  GTest::gtest_main
  sandbox::codegen_warnings
)
add_test(NAME CodegenSexprTests COMMAND codegen-sexpr-tests)
set_tests_properties(CodegenSexprTests PROPERTIES LABELS "unit;all;codegen")

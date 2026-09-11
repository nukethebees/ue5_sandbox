add_executable(material-gen-tests)
target_sources(material-gen-tests PRIVATE material_tests.cpp)
target_link_libraries(material-gen-tests PRIVATE
  material-gen
  GTest::gtest_main
  sandbox::codegen_warnings
)
target_compile_definitions(material-gen-tests PRIVATE
  SANDBOX_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME MaterialGenTests COMMAND material-gen-tests)
set_tests_properties(MaterialGenTests PROPERTIES LABELS "unit;all;codegen")

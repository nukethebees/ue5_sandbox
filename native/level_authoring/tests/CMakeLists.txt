add_executable(native-level-authoring-tests)
target_sources(native-level-authoring-tests PRIVATE level_authoring_tests.cpp)
target_compile_features(native-level-authoring-tests PRIVATE cxx_std_23)
target_link_libraries(native-level-authoring-tests PRIVATE
  native-level-authoring
  GTest::gtest_main
  sandbox::warnings_as_errors
)
configure_sandbox_s7_abi(native-level-authoring-tests)

add_test(NAME native-level-authoring-tests COMMAND native-level-authoring-tests)
set_tests_properties(native-level-authoring-tests PROPERTIES
  LABELS "unit;all;native;native-level-authoring")

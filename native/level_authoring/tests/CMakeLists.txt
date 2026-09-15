add_executable(native-level-authoring-tests)
target_sources(native-level-authoring-tests PRIVATE level_authoring_tests.cpp)
target_compile_features(native-level-authoring-tests PRIVATE cxx_std_23)
target_link_libraries(native-level-authoring-tests PRIVATE
  native-level-authoring
  GTest::gtest_main
  sandbox::warnings_as_errors
)
configure_sandbox_s7_abi(native-level-authoring-tests)

include(GoogleTest)
gtest_discover_tests(native-level-authoring-tests PROPERTIES
  LABELS "unit;all;native-level-authoring")

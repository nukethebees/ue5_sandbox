add_executable(native-level-authoring-tests)
ioj_stage_tbb_runtime(native-level-authoring-tests)
target_sources(native-level-authoring-tests
  PRIVATE
    level_authoring_tests.cpp
)
target_compile_definitions(native-level-authoring-tests
  PRIVATE
    IOJ_LEVEL_SCRIPTS="${PROJECT_SOURCE_DIR}/LevelScripts"
)
target_link_libraries(native-level-authoring-tests
  PRIVATE
    native-level-authoring-s7
    GTest::gtest_main
    sandbox::native_defaults
    ioj::test_pch
)
ioj_enable_warnings(native-level-authoring-tests)
ioj_apply_native_abi(native-level-authoring-tests)

add_test(NAME native-level-authoring-tests COMMAND native-level-authoring-tests)
set_tests_properties(native-level-authoring-tests PROPERTIES
  LABELS "unit;all;native;native-level-authoring")

add_executable(native-level-parser-tests)
target_sources(native-level-parser-tests
  PRIVATE
    parser_tests.cpp
)
target_link_libraries(native-level-parser-tests
  PRIVATE
    native-level-authoring
    GTest::gtest_main
)
ioj_enable_warnings(native-level-parser-tests)
ioj_apply_native_abi(native-level-parser-tests)
add_test(NAME native-level-parser-tests COMMAND native-level-parser-tests)
set_tests_properties(native-level-parser-tests PROPERTIES LABELS "unit;all;native;native-level-authoring")

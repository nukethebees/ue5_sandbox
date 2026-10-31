add_executable(native-levels-tests)
ioj_stage_runtime_dependencies(native-levels-tests)

target_sources(native-levels-tests
  PRIVATE
    input_response_tests.cpp
    launch_state_tests.cpp
)
target_link_libraries(native-levels-tests
  PRIVATE
    native-levels
    GTest::gtest_main
    ioj::test_pch
)
ioj_enable_warnings(native-levels-tests)
ioj_apply_native_abi(native-levels-tests)

add_test(NAME native-levels-tests COMMAND native-levels-tests)
set_tests_properties(native-levels-tests PROPERTIES LABELS "unit;all;native;native-levels")

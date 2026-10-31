add_executable(native-profiles-tests)
ioj_stage_runtime_dependencies(native-profiles-tests)
target_sources(native-profiles-tests
  PRIVATE
    profile_tests.cpp
    profile_storage_tests.cpp
    browser_tests.cpp
    progression_tests.cpp
    completion_publication_tests.cpp
)
target_link_libraries(native-profiles-tests
  PRIVATE
    native-profiles
    GTest::gtest_main
    sandbox::native_defaults
    ioj::test_pch
)
ioj_enable_warnings(native-profiles-tests)
add_test(NAME native-profiles-tests COMMAND native-profiles-tests)
set_tests_properties(native-profiles-tests PROPERTIES LABELS "unit;all;native;native-profiles")

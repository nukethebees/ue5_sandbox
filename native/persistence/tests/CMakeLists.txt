add_executable(native-persistence-tests)
ioj_stage_runtime_dependencies(native-persistence-tests)
target_sources(native-persistence-tests
  PRIVATE
    profile_tests.cpp
    profile_storage_tests.cpp
    browser_tests.cpp
    progression_tests.cpp
    completion_publication_tests.cpp
)
target_link_libraries(native-persistence-tests
  PRIVATE
    native-persistence
    GTest::gtest_main
    sandbox::native_defaults
    ioj::test_pch
)
ioj_enable_warnings(native-persistence-tests)
add_test(NAME native-persistence-tests COMMAND native-persistence-tests)
set_tests_properties(native-persistence-tests PROPERTIES LABELS "unit;all;native;native-persistence")

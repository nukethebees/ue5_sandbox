add_executable(native-presentation-tests)
target_sources(native-presentation-tests
  PRIVATE
    boom_audio_library_tests.cpp
    radar_tests.cpp
    soft_target_tests.cpp
)
target_link_libraries(native-presentation-tests
  PRIVATE
    native-presentation
    GTest::gtest_main
    ioj::test_pch
)
ioj_enable_warnings(native-presentation-tests)
ioj_apply_native_abi(native-presentation-tests)
ioj_stage_runtime_dependencies(native-presentation-tests)
add_test(NAME native-presentation-tests COMMAND native-presentation-tests)
set_tests_properties(native-presentation-tests PROPERTIES LABELS "unit;all;native;native-presentation")

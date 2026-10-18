add_executable(native-gfx-tests)
target_sources(native-gfx-tests
  PRIVATE
    space_dust_math_tests.cpp
)
target_link_libraries(native-gfx-tests
  PRIVATE
    ioj::gfx
    GTest::gtest_main
)
ioj_enable_warnings(native-gfx-tests)
ioj_apply_native_abi(native-gfx-tests)
add_test(NAME native-gfx-tests COMMAND native-gfx-tests)
set_tests_properties(native-gfx-tests PROPERTIES LABELS "unit;all;native;native-gfx")

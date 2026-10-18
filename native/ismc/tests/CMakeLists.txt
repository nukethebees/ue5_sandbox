add_executable(native-ismc-tests)
target_sources(native-ismc-tests
  PRIVATE
    transform_tests.cpp
    packing_tests.cpp
)
target_link_libraries(native-ismc-tests
  PRIVATE
    ioj::ismc
    GTest::gtest_main
)
ioj_enable_warnings(native-ismc-tests)
ioj_apply_native_abi(native-ismc-tests)
add_test(NAME native-ismc-tests COMMAND native-ismc-tests)
set_tests_properties(native-ismc-tests PROPERTIES LABELS "unit;all;native;native-ismc")

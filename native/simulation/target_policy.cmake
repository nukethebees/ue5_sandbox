function(add_native_simulation_test target)
  add_executable(${target} ${ARGN})
  ioj_apply_native_abi(${target})
  target_compile_features(${target} PRIVATE cxx_std_23)
  target_link_libraries(${target} PRIVATE
    native-simulation sandbox::native_defaults ioj::test_pch GTest::gtest_main)
  ioj_enable_warnings(${target})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

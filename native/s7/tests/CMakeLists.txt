add_executable(sandbox-s7-tests)
target_sources(sandbox-s7-tests PRIVATE interpreter_tests.cpp)
target_include_directories(sandbox-s7-tests PRIVATE ../lib/src)
target_include_directories(sandbox-s7-tests SYSTEM PRIVATE ../ThirdParty/s7)
target_link_libraries(sandbox-s7-tests PRIVATE
  sandbox-s7
  sandbox::native_defaults
)
ioj_enable_warnings(sandbox-s7-tests)
ioj_apply_native_abi(sandbox-s7-tests)

add_test(NAME sandbox-s7-tests COMMAND sandbox-s7-tests)
set_tests_properties(sandbox-s7-tests PROPERTIES LABELS "unit;all;native;native-s7")

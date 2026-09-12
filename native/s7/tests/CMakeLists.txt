add_executable(sandbox-s7-tests)
target_sources(sandbox-s7-tests PRIVATE interpreter_tests.cpp)
target_compile_features(sandbox-s7-tests PRIVATE cxx_std_23)
target_link_libraries(sandbox-s7-tests PRIVATE
  sandbox-s7
  sandbox::warnings_as_errors
)
configure_sandbox_s7_abi(sandbox-s7-tests)

add_test(NAME sandbox-s7-tests COMMAND sandbox-s7-tests)
set_tests_properties(sandbox-s7-tests PROPERTIES LABELS "unit;all;native-s7")

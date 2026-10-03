add_executable(jobserver-tests)
target_sources(jobserver-tests PRIVATE protocol_tests.cpp ../client/tests/cli_tests.cpp)
target_link_libraries(jobserver-tests PRIVATE jobserver-cli GTest::gtest_main)
ioj_enable_warnings(jobserver-tests)

add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "jobserver" TIMEOUT 120)

if(TARGET jobserver-platform)
  target_sources(jobserver-tests PRIVATE windows/server_tests.cpp)
  target_link_libraries(jobserver-tests PRIVATE jobserver::client jobserver-server)
  target_compile_definitions(jobserver-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_property(TEST jobserver-tests APPEND PROPERTY LABELS integration)

  add_test(NAME jobserver-version COMMAND jobserver --version)
  set_tests_properties(jobserver-version PROPERTIES LABELS "jobserver" PASS_REGULAR_EXPRESSION "jobserver ")
endif()

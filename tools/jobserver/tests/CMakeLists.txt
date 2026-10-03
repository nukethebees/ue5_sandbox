add_executable(jobserver-tests)
target_sources(jobserver-tests PRIVATE protocol_tests.cpp)
target_link_libraries(jobserver-tests PRIVATE jobserver-core GTest::gtest_main)
ioj_enable_warnings(jobserver-tests)

add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "jobserver" TIMEOUT 120)

if(TARGET jobserver-platform)
  add_executable(jobserver-test-server windows/server_main.cpp)
  target_link_libraries(jobserver-test-server PRIVATE jobserver-server)
  ioj_enable_warnings(jobserver-test-server)
endif()

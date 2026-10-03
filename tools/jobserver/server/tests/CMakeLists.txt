add_executable(jobserver-board-tests)
target_sources(jobserver-board-tests PRIVATE board_tests.cpp)
target_link_libraries(jobserver-board-tests PRIVATE jobserver-board GTest::gtest_main)
ioj_enable_warnings(jobserver-board-tests)

add_test(NAME jobserver-board-tests COMMAND jobserver-board-tests)
set_tests_properties(jobserver-board-tests PROPERTIES LABELS "jobserver")

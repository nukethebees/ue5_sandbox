if(NOT WIN32)
  return()
endif()

get_filename_component(IOJ_TEST_WORKTREE_NAME "${CMAKE_SOURCE_DIR}" NAME)
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/test_environment.cmake.in"
  "${CMAKE_BINARY_DIR}/test_environment.cmake"
  @ONLY
)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
  "${CMAKE_BINARY_DIR}/test_environment.cmake")

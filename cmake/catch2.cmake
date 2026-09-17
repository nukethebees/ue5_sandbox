include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/unreal_paths.cmake")

function(add_low_level_test_suite target_name unreal_target test_name_prefix)
  cmake_parse_arguments(test_suite "" "ACTIVITY" "LABELS" ${ARGN})
  if(test_suite_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_low_level_test_suite(${target_name}) received unexpected arguments: "
      "${test_suite_UNPARSED_ARGUMENTS}")
  endif()

  add_unreal_target(${target_name} ${unreal_target})

  sandbox_get_unreal_target_executable_path(test_executable
    "${PROJECT_SOURCE_DIR}" "${UE_PLATFORM}" "${unreal_target}" "${UE_CONFIGURATION}")

  if(test_suite_ACTIVITY)
    sandbox_jobserver_command(test_command "${test_suite_ACTIVITY}"
      "Low-level test: ${unreal_target}")
  endif()

  discover_tests(
    COMMAND ${test_command} "${test_executable}"
    DISCOVERY_ARGS --list-tests --reporter JSON
    DISCOVERY_MATCH "^[ \\t]*\\\"name\\\": \\\"(${test_name_prefix}\\.[^\\\"]+)\\\",$"
    TEST_NAME "\\1"
    TEST_ARGS "\\1"
    TEST_PROPERTIES
    LABELS "unit;all;${test_suite_LABELS}"
    SKIP_REGULAR_EXPRESSION "SKIPPED:"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  )
endfunction()

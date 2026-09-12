function(add_low_level_test_suite target_name unreal_target test_name_prefix)
  cmake_parse_arguments(test_suite "" "ACTIVITY" "LABELS" ${ARGN})
  if(test_suite_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_low_level_test_suite(${target_name}) received unexpected arguments: "
      "${test_suite_UNPARSED_ARGUMENTS}")
  endif()

  add_unreal_target(${target_name} ${unreal_target})

  if(UE_CONFIGURATION STREQUAL "Development")
    set(test_executable
      "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/${UE_PLATFORM}/${unreal_target}/${unreal_target}.exe"
    )
  else()
    set(test_executable
      "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/${UE_PLATFORM}/${unreal_target}/${unreal_target}-${UE_PLATFORM}-${UE_CONFIGURATION}.exe"
    )
  endif()

  if(test_suite_ACTIVITY)
    sandbox_machine_activity_command(test_command "${test_suite_ACTIVITY}"
      "Low-level test: ${unreal_target}")
    list(INSERT test_command 4
      "-DMACHINE_ACTIVITY_BYPASS_ARGUMENT=--list-tests")
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
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endfunction()

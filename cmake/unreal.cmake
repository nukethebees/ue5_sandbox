function(add_unreal_target target_name unreal_target)
  add_custom_target(${target_name}
    COMMAND "${UE_BUILD_SCRIPT}" ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION}
      "-Project=${SANDBOX_UPROJECT}" -WaitMutex
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_automation_test test_name)
  cmake_parse_arguments(PARSE_ARGV 1 automation_test "" "" "FILTERS;LABELS")

  if(automation_test_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_automation_test(${test_name}) received unexpected arguments: "
      "${automation_test_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT automation_test_FILTERS)
    message(FATAL_ERROR "add_unreal_automation_test(${test_name}) requires FILTERS.")
  endif()

  if(NOT automation_test_LABELS)
    message(FATAL_ERROR "add_unreal_automation_test(${test_name}) requires LABELS.")
  endif()

  set(automation_filters ${automation_test_FILTERS})
  list(TRANSFORM automation_filters PREPEND "StartsWith:")
  list(JOIN automation_filters "+" automation_filter_expression)

  set(space_game_test_arguments)
  if(SANDBOX_SPACE_GAME_TEST_MODE)
    if(NOT SANDBOX_SPACE_GAME_TEST_MODE MATCHES "^(Headless|Level)$")
      message(FATAL_ERROR
        "SANDBOX_SPACE_GAME_TEST_MODE must be empty, Headless, or Level.")
    endif()
    list(APPEND space_game_test_arguments
      "-SpaceGameTestMode=${SANDBOX_SPACE_GAME_TEST_MODE}")
  endif()
  if(SANDBOX_SPACE_GAME_TEST_TIME_SCALE)
    if(NOT SANDBOX_SPACE_GAME_TEST_TIME_SCALE MATCHES "^[0-9]+([.][0-9]+)?$")
      message(FATAL_ERROR
        "SANDBOX_SPACE_GAME_TEST_TIME_SCALE must be empty or a positive number.")
    endif()
    if(SANDBOX_SPACE_GAME_TEST_TIME_SCALE LESS_EQUAL 0)
      message(FATAL_ERROR
        "SANDBOX_SPACE_GAME_TEST_TIME_SCALE must be empty or a positive number.")
    endif()
    list(APPEND space_game_test_arguments
      "-SpaceGameTestTimeScale=${SANDBOX_SPACE_GAME_TEST_TIME_SCALE}")
  endif()

  add_test(
    NAME "${test_name}"
    COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
      "-ExecCmds=Automation RunTests ${automation_filter_expression}; Quit"
      -unattended
      -nop4
      -nosplash
      -nullrhi
      -nosound
      -stdout
      -ddc=NoZenLocalFallback
      "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
      ${space_game_test_arguments}
  )

  # Unreal's queued Quit waits for automation completion and exits non-zero on test errors.
  # Also inspect the test output so a future engine regression cannot turn a reported
  # automation failure into a passing CTest result.
  set_tests_properties("${test_name}" PROPERTIES
    FAIL_REGULAR_EXPRESSION "Test Completed\\. Result=\\{Fail\\};TEST COMPLETE\\. EXIT CODE: -[0-9]+"
    LABELS "${automation_test_LABELS}"
    TIMEOUT 900
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endfunction()

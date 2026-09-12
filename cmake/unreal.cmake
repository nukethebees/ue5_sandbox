function(add_unreal_target target_name unreal_target)
  add_custom_target(${target_name}
    COMMAND ${UE_LOCKED_COMMAND_PREFIX}
      "${UE_BUILD_SCRIPT}" ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION}
      "-Project=${SANDBOX_UPROJECT}" -WaitMutex
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
    USES_TERMINAL
    VERBATIM
  )

  if(TARGET native-memory)
    add_dependencies(${target_name} native-memory)
  endif()
  if(TARGET sbx-mimalloc)
    add_dependencies(${target_name} sbx-mimalloc)
  endif()

  if(TARGET native-simulation)
    add_dependencies(${target_name} native-simulation)
  endif()
  if(TARGET sandbox-s7)
    add_dependencies(${target_name} sandbox-s7)
  endif()
  if(TARGET native-core)
    add_dependencies(${target_name} native-core)
  endif()
endfunction()

function(add_unreal_commandlet_target target_name)
  cmake_parse_arguments(PARSE_ARGV 1 commandlet "" "COMMANDLET;COMMENT" "")

  if(commandlet_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_commandlet_target(${target_name}) received unexpected arguments: "
      "${commandlet_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT commandlet_COMMANDLET)
    message(FATAL_ERROR
      "add_unreal_commandlet_target(${target_name}) requires COMMANDLET.")
  endif()

  if(NOT commandlet_COMMENT)
    message(FATAL_ERROR
      "add_unreal_commandlet_target(${target_name}) requires COMMENT.")
  endif()

  sandbox_machine_activity_command(activity_command STANDARD
    "Unreal commandlet: ${commandlet_COMMANDLET}")
  add_custom_target(${target_name}
    COMMAND ${activity_command} "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
      "-run=${commandlet_COMMANDLET}"
      "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
      -ddc=NoZenLocalFallback
      -unattended
      -nop4
      -nosplash
      -nullrhi
      -nosound
      -stdout
    DEPENDS editor
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "${commandlet_COMMENT}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_benchmark_commandlet_target target_name commandlet)
  sandbox_machine_activity_command(activity_command BENCHMARK
    "Unreal benchmark commandlet: ${commandlet}")
  add_custom_target(${target_name}
    COMMAND ${activity_command} "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
      "-run=${commandlet}"
      ${ARGN}
      -AllowCommandletRendering
      -RenderOffscreen
      -unattended
      -nop4
      -nosplash
      -nosound
      -stdout
    DEPENDS editor
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running ${commandlet} with exclusive benchmark access"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_editor_test test_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_test "" "ACTIVITY;TIMEOUT" "ARGUMENTS;LABELS")

  if(editor_test_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_editor_test(${test_name}) received unexpected arguments: "
      "${editor_test_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT editor_test_ARGUMENTS)
    message(FATAL_ERROR "add_unreal_editor_test(${test_name}) requires ARGUMENTS.")
  endif()

  if(NOT editor_test_LABELS)
    message(FATAL_ERROR "add_unreal_editor_test(${test_name}) requires LABELS.")
  endif()

  if(NOT editor_test_TIMEOUT)
    message(FATAL_ERROR "add_unreal_editor_test(${test_name}) requires TIMEOUT.")
  endif()

  if(NOT editor_test_ACTIVITY)
    set(editor_test_ACTIVITY STANDARD)
  endif()
  if(NOT editor_test_ACTIVITY MATCHES "^(STANDARD|BENCHMARK)$")
    message(FATAL_ERROR
      "add_unreal_editor_test(${test_name}) ACTIVITY must be STANDARD or BENCHMARK.")
  endif()
  sandbox_machine_activity_command(activity_command "${editor_test_ACTIVITY}"
    "Unreal test: ${test_name}")

  add_test(
    NAME "${test_name}"
    COMMAND ${activity_command} "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
      ${editor_test_ARGUMENTS}
      -unattended
      -nop4
      -nosplash
      -nosound
      -stdout
      -ddc=NoZenLocalFallback
      "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
  )

  set_tests_properties("${test_name}" PROPERTIES
    LABELS "${editor_test_LABELS}"
    TIMEOUT "${editor_test_TIMEOUT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
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

  add_unreal_editor_test("${test_name}"
    ARGUMENTS
      "-ExecCmds=Automation RunTests ${automation_filter_expression}; Quit"
      -nullrhi
      ${space_game_test_arguments}
    LABELS ${automation_test_LABELS}
    TIMEOUT 900
  )

  # Unreal's queued Quit waits for automation completion and exits non-zero on test errors.
  # Also inspect the test output so a future engine regression cannot turn a reported
  # automation failure into a passing CTest result.
  set_tests_properties("${test_name}" PROPERTIES
    FAIL_REGULAR_EXPRESSION "Test Completed\\. Result=\\{Fail\\};TEST COMPLETE\\. EXIT CODE: -[0-9]+"
  )
endfunction()

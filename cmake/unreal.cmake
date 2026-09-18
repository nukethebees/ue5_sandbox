include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/unreal_arguments.cmake")

function(add_unreal_target target_name unreal_target)
  set(unreal_target_arguments)
  if(unreal_target STREQUAL "SandboxEditor")
    list(APPEND unreal_target_arguments -verify_editor_modules)
  endif()

  add_custom_target(${target_name}
    COMMAND ${UE_JOBSERVER_COMMAND_PREFIX}
      "${POWERSHELL_EXECUTABLE}" -NoProfile -File
      "${PROJECT_SOURCE_DIR}/PowerShell/BuildUnrealTarget.ps1"
      -build_script "${UE_BUILD_SCRIPT}"
      -target ${unreal_target}
      -platform ${UE_PLATFORM}
      -configuration ${UE_CONFIGURATION}
      -project "${SANDBOX_UPROJECT}"
      -native_toolchain "${SANDBOX_NATIVE_TOOLCHAIN}"
      ${unreal_target_arguments}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
    USES_TERMINAL
    VERBATIM
  )

endfunction()

function(add_unreal_editor_target target_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_target "" "ACTIVITY;COMMENT;EXECUTABLE;FOLDER" "ARGUMENTS;DEPENDS")

  if(editor_target_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_editor_target(${target_name}) received unexpected arguments: "
      "${editor_target_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT editor_target_COMMENT)
    message(FATAL_ERROR "add_unreal_editor_target(${target_name}) requires COMMENT.")
  endif()
  if(NOT editor_target_ACTIVITY)
    set(editor_target_ACTIVITY STANDARD)
  endif()
  if(NOT editor_target_ACTIVITY MATCHES "^(STANDARD|BENCHMARK)$")
    message(FATAL_ERROR
      "add_unreal_editor_target(${target_name}) ACTIVITY must be STANDARD or BENCHMARK.")
  endif()
  if(NOT editor_target_EXECUTABLE)
    set(editor_target_EXECUTABLE "${UE_EDITOR_CMD_EXE}")
  endif()

  sandbox_unreal_jobserver_command(activity_command "${editor_target_ACTIVITY}"
    "Unreal target: ${target_name}")
  add_custom_target(${target_name}
    COMMAND ${activity_command} "${editor_target_EXECUTABLE}" "${SANDBOX_UPROJECT}"
      ${editor_target_ARGUMENTS}
    DEPENDS ${editor_target_DEPENDS}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "${editor_target_COMMENT}"
    USES_TERMINAL
    VERBATIM
  )

  if(editor_target_FOLDER)
    set_property(TARGET ${target_name} PROPERTY FOLDER "${editor_target_FOLDER}")
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

  sandbox_unreal_jobserver_command(activity_command STANDARD
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
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "${commandlet_COMMENT}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_benchmark_commandlet_target target_name commandlet)
  sandbox_unreal_jobserver_command(activity_command BENCHMARK
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
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Running ${commandlet} with exclusive benchmark access"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_editor_test test_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_test "NO_LOCAL_DDC" "ACTIVITY;TIMEOUT" "ARGUMENTS;LABELS")

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
  sandbox_unreal_jobserver_command(activity_command "${editor_test_ACTIVITY}"
    "Unreal test: ${test_name}")

  set(editor_test_common_arguments
    -unattended
    -nop4
    -nosplash
    -nosound
    -stdout
  )
  if(NOT editor_test_NO_LOCAL_DDC)
    list(APPEND editor_test_common_arguments
      -ddc=NoZenLocalFallback
      "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
    )
  endif()

  add_test(
    NAME "${test_name}"
    COMMAND ${activity_command} "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
      ${editor_test_ARGUMENTS}
      ${editor_test_common_arguments}
  )

  set_tests_properties("${test_name}" PROPERTIES
    LABELS "${editor_test_LABELS}"
    TIMEOUT "${editor_test_TIMEOUT}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
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

  sandbox_make_automation_filter_expression(automation_filter_expression
    ${automation_test_FILTERS})
  sandbox_make_space_game_test_arguments(space_game_test_arguments
    "${SANDBOX_SPACE_GAME_TEST_TIME_SCALE}")

  add_unreal_editor_test("${test_name}"
    ARGUMENTS
      "-ExecCmds=Automation Now; Automation RunTests ${automation_filter_expression}; Quit"
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

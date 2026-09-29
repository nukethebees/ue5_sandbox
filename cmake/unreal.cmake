include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/unreal_arguments.cmake")

function(add_unreal_target target_name unreal_target)
  add_custom_target(${target_name}
    COMMAND
      agent-task unreal-build
      --build-script "${UE_BUILD_SCRIPT}"
      --target ${unreal_target}
      --platform ${UE_PLATFORM}
      --configuration ${UE_CONFIGURATION}
      --project "${IOJ_UPROJECT}"
      --native-toolchain "${IOJ_NATIVE_TOOLCHAIN}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
    USES_TERMINAL
    VERBATIM
  )

endfunction()

function(add_unreal_editor_target target_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_target "UNATTENDED" "COMMENT;EXECUTABLE;FOLDER" "ARGUMENTS;DEPENDS")

  if(editor_target_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_editor_target(${target_name}) received unexpected arguments: "
      "${editor_target_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT editor_target_COMMENT)
    message(FATAL_ERROR "add_unreal_editor_target(${target_name}) requires COMMENT.")
  endif()
  if(NOT editor_target_EXECUTABLE)
    set(editor_target_EXECUTABLE "${UE_EDITOR_CMD_EXE}")
  endif()

  if(editor_target_UNATTENDED)
    if(NOT "-unattended" IN_LIST editor_target_ARGUMENTS)
      list(APPEND editor_target_ARGUMENTS -unattended)
    endif()
  endif()

  add_custom_target(${target_name}
    COMMAND "${editor_target_EXECUTABLE}" "${IOJ_UPROJECT}"
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

  add_custom_target(${target_name}
    COMMAND "${UE_EDITOR_CMD_EXE}" "${IOJ_UPROJECT}"
      "-run=${commandlet_COMMANDLET}"
      "-LocalDataCachePath=${IOJ_LOCAL_DDC_DIR}"
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
  add_custom_target(${target_name}
    COMMAND "${UE_EDITOR_CMD_EXE}" "${IOJ_UPROJECT}"
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
    COMMENT "Running ${commandlet}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_editor_test test_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_test "NO_LOCAL_DDC" "TIMEOUT" "ARGUMENTS;LABELS")

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
      "-LocalDataCachePath=${IOJ_LOCAL_DDC_DIR}"
    )
  endif()

  add_test(
    NAME "${test_name}"
    COMMAND "${UE_EDITOR_CMD_EXE}" "${IOJ_UPROJECT}"
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
  sandbox_make_automation_exec_commands(automation_exec_commands
    "${automation_filter_expression}")
  sandbox_make_space_game_test_arguments(space_game_test_arguments
    "${IOJ_SPACE_GAME_TEST_TIME_SCALE}")

  add_unreal_editor_test("${test_name}"
    ARGUMENTS
      "${automation_exec_commands}"
      -nullrhi
      ${space_game_test_arguments}
    LABELS ${automation_test_LABELS}
    TIMEOUT 900
  )

  sandbox_set_automation_test_properties("${test_name}")
endfunction()

function(sandbox_set_automation_test_properties test_name)
  # CTest must see Unreal's normal completion marker. Failure expressions take precedence.
  set_tests_properties("${test_name}" PROPERTIES
    FAIL_REGULAR_EXPRESSION "Found 0 automation tests based on;Test Completed\\. Result=\\{Fail\\};Test Completed\\. Result=\\{Error\\};TEST COMPLETE\\. EXIT CODE: -?[1-9][0-9]*"
    PASS_REGULAR_EXPRESSION "TEST COMPLETE\\. EXIT CODE: 0"
  )
endfunction()

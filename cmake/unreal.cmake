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
  )

  # Unreal's queued Quit waits for automation completion and exits non-zero on test errors.
  set_tests_properties("${test_name}" PROPERTIES
    LABELS "${automation_test_LABELS}"
    TIMEOUT 900
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endfunction()

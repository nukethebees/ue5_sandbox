include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/validate_arguments.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/unreal_arguments.cmake")

function(sandbox_project_enables_plugin output project_file plugin_name)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${project_file}")
  file(READ "${project_file}" project_json)
  string(JSON plugin_count LENGTH "${project_json}" Plugins)
  set(enabled OFF)
  if(plugin_count GREATER 0)
    math(EXPR last_plugin "${plugin_count} - 1")
    foreach(index RANGE 0 ${last_plugin})
      string(JSON name GET "${project_json}" Plugins ${index} Name)
      if(name STREQUAL plugin_name)
        string(JSON enabled GET "${project_json}" Plugins ${index} Enabled)
        break()
      endif()
    endforeach()
  endif()
  set(${output} "${enabled}" PARENT_SCOPE)
endfunction()

function(add_unreal_target target_name unreal_target)
  set(cache_start_command)
  set(build_environment)
  if(IOJ_ENABLE_UBA_CACHE)
    set(cache_start_command
      COMMAND pwsh -NoProfile -File "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/start_uba_cache.ps1"
        -EngineRoot "${UE_ROOT_ABSOLUTE}"
        -CapacityGB "${IOJ_UBA_CACHE_CAPACITY_GB}"
    )
    set(build_environment
      "${CMAKE_COMMAND}" -E env
        "UnrealBuildTool_UnrealBuildAccelerator__Cache=127.0.0.1:1347"
        "UnrealBuildTool_UnrealBuildAccelerator__WriteCache=True"
    )
  endif()

  add_custom_target(${target_name}
    ${cache_start_command}
    COMMAND
      ${build_environment} coj unreal-build
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
  cmake_parse_arguments(PARSE_ARGV 1 editor_target "UNATTENDED" "COMMENT;FOLDER" "ARGUMENTS;DEPENDS")

  ioj_validate_arguments(editor_target "add_unreal_editor_target(${target_name})" COMMENT)

  if(editor_target_UNATTENDED)
    if(NOT "-unattended" IN_LIST editor_target_ARGUMENTS)
      list(APPEND editor_target_ARGUMENTS -unattended)
    endif()
  endif()

  add_custom_target(${target_name}
    COMMAND "${UE_EDITOR_CMD_EXE}" "${IOJ_UPROJECT}"
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

function(add_unreal_editor_test test_name)
  cmake_parse_arguments(PARSE_ARGV 1 editor_test "NO_LOCAL_DDC" "TIMEOUT" "ARGUMENTS;LABELS")

  ioj_validate_arguments(editor_test "add_unreal_editor_test(${test_name})" ARGUMENTS LABELS TIMEOUT)

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

  ioj_validate_arguments(automation_test "add_unreal_automation_test(${test_name})" FILTERS LABELS)

  sandbox_make_automation_filter_expression(automation_filter_expression
    ${automation_test_FILTERS})
  sandbox_make_automation_exec_commands(automation_exec_commands
    "${automation_filter_expression}")

  add_unreal_editor_test("${test_name}"
    ARGUMENTS
      "${automation_exec_commands}"
      -nullrhi
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

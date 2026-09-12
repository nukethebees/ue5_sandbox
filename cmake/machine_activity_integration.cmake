include_guard(GLOBAL)

set(SANDBOX_MACHINE_ACTIVITY_MODULE
  "${CMAKE_CURRENT_LIST_DIR}/machine_activity.cmake")
set(SANDBOX_MACHINE_ACTIVITY_RUNNER
  "${CMAKE_CURRENT_LIST_DIR}/run_with_machine_activity.cmake")
include("${SANDBOX_MACHINE_ACTIVITY_MODULE}")

function(sandbox_machine_activity_command output_variable mode operation)
  set(command
    "${CMAKE_COMMAND}"
    "-DMACHINE_ACTIVITY_MODULE=${SANDBOX_MACHINE_ACTIVITY_MODULE}"
    "-DMACHINE_ACTIVITY_MODE=${mode}"
    "-DMACHINE_ACTIVITY_OPERATION=${operation}"
    -P "${SANDBOX_MACHINE_ACTIVITY_RUNNER}"
    --
  )
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

function(sandbox_configure_machine_activity)
  sandbox_machine_activity_acquire(STANDARD
    "CMake configure: ${CMAKE_CURRENT_SOURCE_DIR}")
  cmake_language(DEFER CALL sandbox_machine_activity_release)

  sandbox_machine_activity_command(
    standard_command STANDARD "CMake heavy command")
  sandbox_machine_activity_command(
    benchmark_command BENCHMARK "CMake benchmark")

  sandbox_machine_activity_command(
    compile_launcher STANDARD "C++ compilation")
  list(INSERT compile_launcher 4 -DMACHINE_ACTIVITY_QUIET=ON)
  sandbox_machine_activity_command(
    link_launcher STANDARD "C++ linking")
  list(INSERT link_launcher 4 -DMACHINE_ACTIVITY_QUIET=ON)

  set(standard_activity_command "${standard_command}" PARENT_SCOPE)
  set(benchmark_activity_command "${benchmark_command}" PARENT_SCOPE)
  set(CMAKE_CXX_COMPILER_LAUNCHER
    ${compile_launcher} ${CMAKE_CXX_COMPILER_LAUNCHER}
    PARENT_SCOPE
  )
  set(CMAKE_CXX_LINKER_LAUNCHER
    ${link_launcher} ${CMAKE_CXX_LINKER_LAUNCHER}
    PARENT_SCOPE
  )
endfunction()

function(sandbox_add_machine_activity_status_target)
  add_custom_target(machine-activity-status
    COMMAND "${CMAKE_COMMAND}"
      "-DMACHINE_ACTIVITY_MODULE=${SANDBOX_MACHINE_ACTIVITY_MODULE}"
      -DMACHINE_ACTIVITY_ACTION=status
      -P "${SANDBOX_MACHINE_ACTIVITY_RUNNER}"
    COMMENT "Showing machine-wide Sandbox activity gate state"
    VERBATIM
  )
endfunction()

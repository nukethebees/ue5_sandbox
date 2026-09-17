include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_commands.cmake")

if(NOT DEFINED ENV{LOCALAPPDATA} OR "$ENV{LOCALAPPDATA}" STREQUAL "")
  message(FATAL_ERROR "LOCALAPPDATA is required to locate the per-user jobserver.")
endif()

cmake_path(SET SANDBOX_JOBSERVER_INSTALL_ROOT NORMALIZE
  "$ENV{LOCALAPPDATA}/NukeTheBees/jobserver")
cmake_path(APPEND SANDBOX_JOBSERVER_INSTALL_ROOT bin jobserver.exe
  OUTPUT_VARIABLE SANDBOX_JOBSERVER_CLI)

function(sandbox_jobserver_command output_variable mode operation)
  sandbox_make_jobserver_command(command "${SANDBOX_JOBSERVER_CLI}"
    "${CMAKE_SOURCE_DIR}" "${mode}" "${operation}")
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

function(sandbox_configure_jobserver)
  sandbox_jobserver_command(
    standard_command STANDARD "CMake heavy command")
  sandbox_jobserver_command(
    benchmark_command BENCHMARK "CMake benchmark")

  set(standard_activity_command "${standard_command}" PARENT_SCOPE)
  set(benchmark_activity_command "${benchmark_command}" PARENT_SCOPE)

  if(EXISTS "${SANDBOX_JOBSERVER_CLI}")
    sandbox_jobserver_command(
      compile_launcher STANDARD "C++ compilation")
    sandbox_jobserver_command(
      link_launcher STANDARD "C++ linking")
    set(CMAKE_CXX_COMPILER_LAUNCHER
      ${compile_launcher} ${CMAKE_CXX_COMPILER_LAUNCHER}
      PARENT_SCOPE
    )
    set(CMAKE_CXX_LINKER_LAUNCHER
      ${link_launcher} ${CMAKE_CXX_LINKER_LAUNCHER}
      PARENT_SCOPE
    )
  else()
    message(STATUS
      "The per-user jobserver is not installed; bootstrap targets will build "
      "without cross-worktree scheduling. Run the install-jobserver target.")
  endif()
endfunction()

function(sandbox_add_jobserver_status_target)
  add_custom_target(jobserver-status
    COMMAND "${SANDBOX_JOBSERVER_CLI}" status
    COMMENT "Showing per-user jobserver state"
    VERBATIM
  )
endfunction()

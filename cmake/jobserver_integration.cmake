include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_commands.cmake")

if(NOT DEFINED ENV{LOCALAPPDATA} OR "$ENV{LOCALAPPDATA}" STREQUAL "")
  message(FATAL_ERROR "LOCALAPPDATA is required to locate the per-user jobserver.")
endif()

cmake_path(SET IOJ_JOBSERVER_INSTALL_ROOT NORMALIZE
  "$ENV{LOCALAPPDATA}/NukeTheBees/jobserver")
cmake_path(APPEND IOJ_JOBSERVER_INSTALL_ROOT bin jobserver.exe OUTPUT_VARIABLE IOJ_JOBSERVER_CLI)

function(sandbox_jobserver_command output_variable mode kind operation)
  sandbox_make_jobserver_command(command "${IOJ_JOBSERVER_CLI}"
    "${CMAKE_SOURCE_DIR}" "${mode}" "${kind}" "${operation}")
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

function(sandbox_add_jobserver_status_target)
  add_custom_target(jobserver-status
    COMMAND "${IOJ_JOBSERVER_CLI}" status
    COMMENT "Showing per-user jobserver state"
    VERBATIM
  )
endfunction()

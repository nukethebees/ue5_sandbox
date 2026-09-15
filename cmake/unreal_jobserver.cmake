include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_integration.cmake")

function(sandbox_configure_unreal_jobserver engine_root)
  file(REAL_PATH "${engine_root}" canonical_root BASE_DIRECTORY "${PROJECT_SOURCE_DIR}")
  cmake_path(NORMAL_PATH canonical_root)
  string(TOLOWER "${canonical_root}" identity)
  string(SHA256 resource_hash "${identity}")
  set(resource "unreal-build/${resource_hash}")
  set(UE_ENGINE_JOBSERVER_RESOURCE "${resource}" PARENT_SCOPE)

  set(build_command
    "${SANDBOX_JOBSERVER_CLI}" run
    --name "Unreal build: ${canonical_root}"
    --kind unreal-build
    --worktree "${PROJECT_SOURCE_DIR}"
    --shared machine
    --exclusive "${resource}"
    --)
  set(UE_JOBSERVER_COMMAND_PREFIX "${build_command}" PARENT_SCOPE)
endfunction()

function(sandbox_unreal_jobserver_command output_variable mode operation)
  if(NOT UE_ENGINE_JOBSERVER_RESOURCE)
    message(FATAL_ERROR "Configure the Unreal jobserver engine resource before creating commands.")
  endif()
  sandbox_jobserver_command(command "${mode}" "${operation}")
  list(POP_BACK command)
  list(APPEND command --shared "${UE_ENGINE_JOBSERVER_RESOURCE}" --)
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

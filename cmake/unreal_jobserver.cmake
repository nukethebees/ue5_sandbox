include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_integration.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/unreal_paths.cmake")

function(sandbox_configure_unreal_jobserver engine_root)
  file(REAL_PATH "${engine_root}" canonical_root BASE_DIRECTORY "${PROJECT_SOURCE_DIR}")
  cmake_path(NORMAL_PATH canonical_root)
  sandbox_make_unreal_jobserver_resource(resource "${canonical_root}")
  set(UE_ENGINE_JOBSERVER_RESOURCE "${resource}" CACHE INTERNAL
    "Canonical jobserver resource for the configured Unreal engine" FORCE)
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

function(sandbox_unreal_jobserver_command output_variable mode engine_access operation)
  if(NOT UE_ENGINE_JOBSERVER_RESOURCE)
    message(FATAL_ERROR "Configure the Unreal jobserver engine resource before creating commands.")
  endif()
  if(NOT engine_access MATCHES "^(SHARED|EXCLUSIVE)$")
    message(FATAL_ERROR
      "Unreal jobserver engine access must be SHARED or EXCLUSIVE, got '${engine_access}'.")
  endif()

  sandbox_jobserver_command(command "${mode}" "${operation}")
  list(POP_BACK command)
  if(engine_access STREQUAL "SHARED")
    list(APPEND command --shared "${UE_ENGINE_JOBSERVER_RESOURCE}" --)
  else()
    list(APPEND command --exclusive "${UE_ENGINE_JOBSERVER_RESOURCE}" --)
  endif()
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

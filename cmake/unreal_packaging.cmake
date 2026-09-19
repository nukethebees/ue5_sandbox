include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/unreal_arguments.cmake")

function(add_unreal_build_cook_run_target target_name)
  cmake_parse_arguments(PARSE_ARGV 1 uat "" "COMMENT" "ARGUMENTS")

  if(uat_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unreal_build_cook_run_target(${target_name}) received unexpected arguments: "
      "${uat_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT uat_COMMENT)
    message(FATAL_ERROR
      "add_unreal_build_cook_run_target(${target_name}) requires COMMENT.")
  endif()

  add_custom_target(${target_name}
    COMMAND ${UE_JOBSERVER_COMMAND_PREFIX}
      "${CMAKE_COMMAND}" -E env
      "SANDBOX_NATIVE_TOOLCHAIN=${SANDBOX_NATIVE_TOOLCHAIN}"
      "UE-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
      "${UE_RUN_UAT_SCRIPT}" BuildCookRun
      "-project=${SANDBOX_UPROJECT}"
      -target=Sandbox
      "-targetplatform=${UE_PLATFORM}"
      "-clientconfig=${UE_CONFIGURATION}"
      -skipbuild
      -nop4
      -unattended
      -utf8output
      ${uat_ARGUMENTS}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "${uat_COMMENT}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(add_unreal_packaging_targets)
  if(NOT UE_PLATFORM STREQUAL "Win64")
    message(FATAL_ERROR
      "The initial Unreal packaging targets support UE_PLATFORM=Win64 only. "
      "Got '${UE_PLATFORM}'.")
  endif()

  sandbox_make_unreal_cook_arguments(cook_arguments
    "${UE_DEVELOPMENT_EDITOR_CMD_EXE}" "${SANDBOX_GAME_COOK_DIRECTORY}")

  if(UE_CONFIGURATION STREQUAL "Development")
    add_unreal_build_cook_run_target(cook
      COMMENT "Clean-cooking Sandbox content for ${UE_PLATFORM}"
      ARGUMENTS ${cook_arguments} -clean
    )
    add_dependencies(cook editor)

    add_unreal_build_cook_run_target(cook-incremental
      COMMENT "Incrementally cooking Sandbox content for ${UE_PLATFORM}"
      ARGUMENTS ${cook_arguments} -iterate
    )
    add_dependencies(cook-incremental editor)
  endif()

  sandbox_make_unreal_container_arguments(container_arguments
    "${SANDBOX_GAME_COOK_ROOT}" "${SANDBOX_GAME_STAGE_ROOT}")

  add_unreal_build_cook_run_target(stage
    COMMENT "Staging Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    ARGUMENTS
      -skipcook
      -stage
      ${container_arguments}
  )

  add_unreal_build_cook_run_target(archive
    COMMENT "Archiving Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    ARGUMENTS
      -skipcook
      -skipstage
      -archive
      ${container_arguments}
      "-archivedirectory=${SANDBOX_GAME_ARCHIVE_ROOT}"
  )

  add_custom_target(run-staged
    COMMAND ${standard_activity_command} "${CMAKE_COMMAND}" -E chdir
      "${SANDBOX_GAME_STAGE_DIRECTORY}"
      "${SANDBOX_GAME_STAGE_EXECUTABLE}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Running staged Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(verify-package
    COMMAND ${unreal_standard_activity_command} "${SANDBOX_GAME_PACKAGE_TOOLS}"
      --project-root "${PROJECT_SOURCE_DIR}"
      --package-root "${SANDBOX_GAME_ARCHIVE_DIRECTORY}"
      --unreal-pak "${UE_UNREAL_PAK_EXE}"
      --verification-directory "${SANDBOX_GAME_VERIFICATION_ROOT}"
      --configuration "${UE_CONFIGURATION}"
    DEPENDS sandbox-game-package-tools-preflight
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Verifying archived Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

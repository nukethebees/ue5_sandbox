include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/validate_arguments.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/unreal_arguments.cmake")

function(add_unreal_build_cook_run_target target_name)
  cmake_parse_arguments(PARSE_ARGV 1 uat "" "COMMENT" "ARGUMENTS")

  ioj_validate_arguments(uat "add_unreal_build_cook_run_target(${target_name})" COMMENT)

  add_custom_target(${target_name}
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "IOJ_NATIVE_TOOLCHAIN=${IOJ_NATIVE_TOOLCHAIN}"
      cmd.exe /d /c call "${UE_RUN_UAT_SCRIPT}" BuildCookRun
      "-project=${IOJ_UPROJECT}"
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
    "${UE_DEVELOPMENT_EDITOR_CMD_EXE}" "${IOJ_GAME_COOK_DIRECTORY}")

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
    "${IOJ_GAME_COOK_ROOT}" "${IOJ_GAME_STAGE_ROOT}")

  add_unreal_build_cook_run_target(stage
    COMMENT "Staging Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    ARGUMENTS
      -skipcook
      -stage
      -NoBootstrapExe
      ${container_arguments}
  )

  add_dependencies(stage IojPlayer)
  cmake_path(GET IOJ_GAME_STAGE_EXECUTABLE PARENT_PATH staged_host_directory)
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/IojPlayer.path"
    CONTENT "${IOJ_GAME_STAGE_EXECUTABLE}\n")
  add_custom_command(TARGET stage POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:IojPlayer>" "${staged_host_directory}/"
    COMMAND "${CMAKE_COMMAND}"
      "-Druntime_dlls=$<TARGET_RUNTIME_DLLS:IojPlayer>"
      "-Ddestination=${staged_host_directory}"
      -P "${PROJECT_SOURCE_DIR}/cmake/stage_runtime_dependencies.cmake"
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${PROJECT_SOURCE_DIR}/Binaries/Win64/${IOJ_GAME_DLL_NAME}" "${staged_host_directory}/${IOJ_GAME_DLL_NAME}"
    VERBATIM
  )

  if(UE_CONFIGURATION STREQUAL "Development")
    add_custom_command(TARGET stage POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/Saved"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/IojPlayer.path" "${PROJECT_SOURCE_DIR}/Saved/IojPlayer.path"
      VERBATIM
    )
  endif()

  add_unreal_build_cook_run_target(archive
    COMMENT "Archiving Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    ARGUMENTS
      -skipcook
      -skipstage
      -NoBootstrapExe
      -archive
      ${container_arguments}
      "-archivedirectory=${IOJ_GAME_ARCHIVE_ROOT}"
  )

  # UAT updates archives in place. Retire the two executables from an older stock package.
  cmake_path(REPLACE_EXTENSION IOJ_GAME_DLL_NAME ".exe" OUTPUT_VARIABLE stock_game_executable)
  add_custom_command(TARGET archive POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${IOJ_GAME_ARCHIVE_DIRECTORY}/Sandbox.exe"
      "${IOJ_GAME_ARCHIVE_DIRECTORY}/Sandbox/Binaries/Win64/${stock_game_executable}"
    VERBATIM
  )

  add_custom_target(verify-package
    COMMAND "${IOJ_GAME_PACKAGE_TOOLS}"
      --project-root "${PROJECT_SOURCE_DIR}"
      --package-root "${IOJ_GAME_ARCHIVE_DIRECTORY}"
      --unreal-pak "${UE_UNREAL_PAK_EXE}"
      --verification-directory "${IOJ_GAME_VERIFICATION_ROOT}"
      --configuration "${UE_CONFIGURATION}"
    DEPENDS game-package-tools-host
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Verifying archived Sandbox ${UE_PLATFORM} ${UE_CONFIGURATION}"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

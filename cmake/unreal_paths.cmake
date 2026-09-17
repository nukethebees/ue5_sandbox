include_guard(GLOBAL)

function(sandbox_resolve_unreal_root absolute_output canonical_output root base_directory)
  cmake_path(ABSOLUTE_PATH root BASE_DIRECTORY "${base_directory}" NORMALIZE
    OUTPUT_VARIABLE absolute_root)
  file(REAL_PATH "${absolute_root}" canonical_root)
  cmake_path(NORMAL_PATH canonical_root)
  set(${absolute_output} "${absolute_root}" PARENT_SCOPE)
  set(${canonical_output} "${canonical_root}" PARENT_SCOPE)
endfunction()

function(sandbox_get_unreal_editor_names editor_output editor_cmd_output configuration)
  if(configuration MATCHES "^(Debug|DebugGame|Development)$")
    set(editor_configuration "${configuration}")
  else()
    set(editor_configuration Development)
  endif()

  if(editor_configuration STREQUAL "Development")
    set(editor_name UnrealEditor.exe)
    set(editor_cmd_name UnrealEditor-Cmd.exe)
  else()
    set(editor_name "UnrealEditor-Win64-${editor_configuration}.exe")
    set(editor_cmd_name "UnrealEditor-Win64-${editor_configuration}-Cmd.exe")
  endif()

  set(${editor_output} "${editor_name}" PARENT_SCOPE)
  set(${editor_cmd_output} "${editor_cmd_name}" PARENT_SCOPE)
endfunction()

function(sandbox_configure_unreal_engine_paths prefix engine_root editor_name editor_cmd_name)
  cmake_path(APPEND engine_root Engine Build BatchFiles RunUBT.bat
    OUTPUT_VARIABLE build_script)
  cmake_path(APPEND engine_root Engine Build BatchFiles RunUAT.bat
    OUTPUT_VARIABLE run_uat_script)
  cmake_path(APPEND engine_root Engine Build BatchFiles GenerateProjectFiles.bat
    OUTPUT_VARIABLE generate_project_files_script)
  cmake_path(APPEND engine_root Engine Binaries Win64 "${editor_name}"
    OUTPUT_VARIABLE editor_executable)
  cmake_path(APPEND engine_root Engine Binaries Win64 "${editor_cmd_name}"
    OUTPUT_VARIABLE editor_cmd_executable)
  cmake_path(APPEND engine_root Engine Binaries Win64 UnrealEditor-Cmd.exe
    OUTPUT_VARIABLE development_editor_cmd_executable)
  cmake_path(APPEND engine_root Engine Binaries Win64 UnrealPak.exe
    OUTPUT_VARIABLE unreal_pak_executable)

  set(${prefix}_BUILD_SCRIPT "${build_script}" PARENT_SCOPE)
  set(${prefix}_RUN_UAT_SCRIPT "${run_uat_script}" PARENT_SCOPE)
  set(${prefix}_GENERATE_PROJECT_FILES_SCRIPT "${generate_project_files_script}" PARENT_SCOPE)
  set(${prefix}_EDITOR_EXE "${editor_executable}" PARENT_SCOPE)
  set(${prefix}_EDITOR_CMD_EXE "${editor_cmd_executable}" PARENT_SCOPE)
  set(${prefix}_DEVELOPMENT_EDITOR_CMD_EXE "${development_editor_cmd_executable}" PARENT_SCOPE)
  set(${prefix}_UNREAL_PAK_EXE "${unreal_pak_executable}" PARENT_SCOPE)
endfunction()

function(sandbox_get_unreal_target_executable_path output_variable project_root platform target configuration)
  if(configuration STREQUAL "Development")
    set(executable_name "${target}.exe")
  else()
    set(executable_name "${target}-${platform}-${configuration}.exe")
  endif()

  cmake_path(APPEND project_root Binaries "${platform}" "${target}" "${executable_name}"
    OUTPUT_VARIABLE executable_path)
  set(${output_variable} "${executable_path}" PARENT_SCOPE)
endfunction()

function(sandbox_configure_game_artifact_paths prefix artifact_root base_directory platform configuration)
  cmake_path(ABSOLUTE_PATH artifact_root BASE_DIRECTORY "${base_directory}" NORMALIZE
    OUTPUT_VARIABLE artifact_root_absolute)
  set(platform_root "${artifact_root_absolute}/${platform}")
  set(configuration_root "${platform_root}/${configuration}")
  set(${prefix}_ROOT "${artifact_root_absolute}" PARENT_SCOPE)
  set(${prefix}_COOK_ROOT "${platform_root}/cook" PARENT_SCOPE)
  set(${prefix}_COOK_DIRECTORY "${platform_root}/cook/Windows" PARENT_SCOPE)
  set(${prefix}_STAGE_ROOT "${configuration_root}/stage" PARENT_SCOPE)
  set(${prefix}_ARCHIVE_ROOT "${configuration_root}/archive" PARENT_SCOPE)
  set(${prefix}_STAGE_DIRECTORY "${configuration_root}/stage/Windows" PARENT_SCOPE)
  set(${prefix}_ARCHIVE_DIRECTORY "${configuration_root}/archive" PARENT_SCOPE)
  set(${prefix}_STAGE_EXECUTABLE "${configuration_root}/stage/Windows/Sandbox.exe" PARENT_SCOPE)
  set(${prefix}_VERIFICATION_ROOT "${configuration_root}/verification" PARENT_SCOPE)
endfunction()

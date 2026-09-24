include("${PROJECT_SOURCE_DIR}/cmake/unreal_paths.cmake")

foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
  sandbox_get_unreal_editor_names(editor_name editor_cmd_name "${configuration}")
  if(configuration STREQUAL "Debug")
    set(expected_editor UnrealEditor-Win64-Debug.exe)
    set(expected_editor_cmd UnrealEditor-Win64-Debug-Cmd.exe)
  elseif(configuration STREQUAL "DebugGame")
    set(expected_editor UnrealEditor-Win64-DebugGame.exe)
    set(expected_editor_cmd UnrealEditor-Win64-DebugGame-Cmd.exe)
  else()
    set(expected_editor UnrealEditor.exe)
    set(expected_editor_cmd UnrealEditor-Cmd.exe)
  endif()
  sandbox_assert_equal("${configuration} editor name" "${editor_name}" "${expected_editor}")
  sandbox_assert_equal("${configuration} editor command name"
    "${editor_cmd_name}" "${expected_editor_cmd}")
endforeach()

set(cmake_test_root "${CMAKE_CURRENT_BINARY_DIR}/root with spaces")
file(MAKE_DIRECTORY "${cmake_test_root}/Engine")
sandbox_resolve_unreal_root(root_absolute root_canonical
  "${cmake_test_root}/Engine/.." "${CMAKE_CURRENT_BINARY_DIR}")
cmake_path(NORMAL_PATH cmake_test_root OUTPUT_VARIABLE expected_root)
sandbox_assert_equal("Unreal root resolves dot segments"
  "${root_absolute}" "${expected_root}/")
sandbox_assert_equal("Unreal root canonicalizes an existing directory"
  "${root_canonical}" "${expected_root}")

sandbox_configure_game_artifact_paths(game_artifact
  [=[C:\Artifacts Root\Sandbox]=] [=[C:\ignored]=] Win64 DebugGame)
sandbox_assert_equal("artifact cook path"
  "${game_artifact_COOK_DIRECTORY}" [=[C:/Artifacts Root/Sandbox/Win64/cook/Windows]=])
sandbox_assert_equal("artifact stage executable path"
  "${game_artifact_STAGE_EXECUTABLE}"
  [=[C:/Artifacts Root/Sandbox/Win64/DebugGame/stage/Windows/Sandbox.exe]=])
sandbox_assert_equal("artifact verification path"
  "${game_artifact_VERIFICATION_ROOT}"
  [=[C:/Artifacts Root/Sandbox/Win64/DebugGame/verification]=])

sandbox_configure_unreal_engine_paths(engine
  [=[C:\Engine Root\UE 5.8]=] UnrealEditor-Win64-DebugGame.exe
  UnrealEditor-Win64-DebugGame-Cmd.exe)
sandbox_assert_equal("Unreal build script path" "${engine_BUILD_SCRIPT}"
  [=[C:/Engine Root/UE 5.8/Engine/Build/BatchFiles/RunUBT.bat]=])
sandbox_assert_equal("Unreal editor command path" "${engine_EDITOR_CMD_EXE}"
  [=[C:/Engine Root/UE 5.8/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe]=])
sandbox_assert_equal("UnrealPak path" "${engine_UNREAL_PAK_EXE}"
  [=[C:/Engine Root/UE 5.8/Engine/Binaries/Win64/UnrealPak.exe]=])

set(source_engine_root "${CMAKE_CURRENT_BINARY_DIR}/source engine")
set(source_engine_batch_files "${source_engine_root}/Engine/Build/BatchFiles")
file(MAKE_DIRECTORY "${source_engine_batch_files}")
file(TOUCH
  "${source_engine_batch_files}/RunUBT.bat"
  "${source_engine_batch_files}/Build.bat"
  "${source_engine_batch_files}/GenerateProjectFiles.bat")
sandbox_configure_unreal_engine_paths(source_engine "${source_engine_root}"
  UnrealEditor.exe UnrealEditor-Cmd.exe)
sandbox_assert_equal("Source Unreal build script path" "${source_engine_BUILD_SCRIPT}"
  "${source_engine_batch_files}/RunUBT.bat")
sandbox_assert_equal("Source Unreal project-files script path"
  "${source_engine_GENERATE_PROJECT_FILES_SCRIPT}"
  "${source_engine_batch_files}/GenerateProjectFiles.bat")
sandbox_assert_equal("Source Unreal project-files arguments"
  "${source_engine_GENERATE_PROJECT_FILES_ARGUMENTS}" "")

set(installed_engine_root "${CMAKE_CURRENT_BINARY_DIR}/installed engine")
set(installed_engine_batch_files
  "${installed_engine_root}/Engine/Build/BatchFiles")
file(MAKE_DIRECTORY "${installed_engine_batch_files}")
file(TOUCH "${installed_engine_batch_files}/Build.bat")
sandbox_configure_unreal_engine_paths(installed_engine "${installed_engine_root}"
  UnrealEditor.exe UnrealEditor-Cmd.exe)
sandbox_assert_equal("Installed Unreal build script path" "${installed_engine_BUILD_SCRIPT}"
  "${installed_engine_batch_files}/Build.bat")
sandbox_assert_equal("Installed Unreal project-files script path"
  "${installed_engine_GENERATE_PROJECT_FILES_SCRIPT}"
  "${installed_engine_batch_files}/Build.bat")
sandbox_assert_list_equal("Installed Unreal project-files arguments"
  "${installed_engine_GENERATE_PROJECT_FILES_ARGUMENTS}" -ProjectFiles)

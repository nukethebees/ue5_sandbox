include("${CMAKE_CURRENT_LIST_DIR}/assertions.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/jobserver_commands.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/unreal_paths.cmake")

set(cmake_test_cli [=[C:\Users\Test User\jobserver.exe]=])
set(cmake_test_worktree [=[C:\src\Sandbox Worktree]=])
sandbox_make_jobserver_command(standard_command "${cmake_test_cli}"
  "${cmake_test_worktree}" STANDARD "Editor command α")
set(expected_standard_command
  "${cmake_test_cli}" run
  --name "Editor command α"
  --kind standard
  --worktree "${cmake_test_worktree}"
  --shared machine
  --
)
sandbox_assert_list_equal("standard jobserver command preserves Windows paths"
  "${standard_command}" "${expected_standard_command}")

sandbox_make_jobserver_command(benchmark_command "${cmake_test_cli}"
  "${cmake_test_worktree}" BENCHMARK "Benchmark command")
set(expected_benchmark_command
  "${cmake_test_cli}" run
  --name "Benchmark command"
  --kind benchmark
  --worktree "${cmake_test_worktree}"
  --exclusive machine
  --exclusive benchmark
  --
)
sandbox_assert_list_equal("benchmark jobserver command claims exclusive resources"
  "${benchmark_command}" "${expected_benchmark_command}")

sandbox_make_unreal_jobserver_resource(resource
  [=[C:\Engine Root\UE 5.8]=])
string(SHA256 expected_resource_hash [=[c:\engine root\ue 5.8]=])
sandbox_assert_equal("Unreal resource identity is case-insensitive"
  "${resource}" "unreal-build/${expected_resource_hash}")

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

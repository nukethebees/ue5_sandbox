include("${CMAKE_CURRENT_LIST_DIR}/assertions.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/jobserver_commands.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/unreal_paths.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/unreal_arguments.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/msvc_environment.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/clang_cl_asan_paths.cmake")

set(cmake_test_cli [=[C:\Users\Test User\jobserver.exe]=])
set(cmake_test_worktree [=[C:\src\Sandbox Worktree]=])
sandbox_make_jobserver_command(standard_command "${cmake_test_cli}"
  "${cmake_test_worktree}" STANDARD command "Editor command α")
set(expected_standard_command
  "${cmake_test_cli}" run
  --name "Editor command α"
  --kind command
  --worktree "${cmake_test_worktree}"
  --shared machine
  --
)
sandbox_assert_list_equal("standard jobserver command preserves Windows paths"
  "${standard_command}" "${expected_standard_command}")

sandbox_make_jobserver_command(benchmark_command "${cmake_test_cli}"
  "${cmake_test_worktree}" BENCHMARK benchmark "Benchmark command")
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

sandbox_make_automation_filter_expression(automation_expression
  Sandbox.UnitTests Sandbox.LevelTests.PlayerInputSmoke)
sandbox_assert_equal("automation filters preserve order"
  "${automation_expression}"
  "StartsWith:Sandbox.UnitTests+StartsWith:Sandbox.LevelTests.PlayerInputSmoke")
sandbox_make_space_game_test_arguments(empty_time_scale_arguments "")
sandbox_assert_list_equal("empty time scale adds no arguments"
  "${empty_time_scale_arguments}" "")
sandbox_make_space_game_test_arguments(time_scale_arguments 1.25)
sandbox_assert_list_equal("positive time scale is a single argument"
  "${time_scale_arguments}" "-SpaceGameTestTimeScale=1.25")

sandbox_make_unreal_cook_arguments(cook_arguments
  [=[C:\Engine Root\UnrealEditor-Cmd.exe]=]
  [=[C:\Artifacts Root\cook\Windows]=])
sandbox_assert_list_equal("cook arguments preserve Windows paths" "${cook_arguments}"
  [=[-cook;-skipstage;-unrealexe=C:\Engine Root\UnrealEditor-Cmd.exe;-ddc=NoZenLocalFallback;-cookoutputdir=C:\Artifacts Root\cook\Windows]=])
sandbox_make_unreal_container_arguments(container_arguments
  [=[C:\Artifacts Root\cook]=] [=[C:\Artifacts Root\stage]=])
sandbox_assert_list_equal("container arguments preserve Windows paths" "${container_arguments}"
  [=[-pak;-iostore;-compressed;-prereqs;-cookoutputdir=C:\Artifacts Root\cook;-stagingdirectory=C:\Artifacts Root\stage]=])

set(msvc_environment_text [=[noise
SANDBOX_MSVC_PATH=C:\VS Tools\bin;C:\Windows\System32
SANDBOX_MSVC_INCLUDE= C:\SDK Include\ucrt
]=])
string(REPLACE "\n" "\r\n" msvc_environment_text "${msvc_environment_text}")
sandbox_get_msvc_environment_value(msvc_path "${msvc_environment_text}" PATH)
sandbox_assert_equal("MSVC PATH retains semicolon-separated Windows paths"
  "${msvc_path}" [=[C:\VS Tools\bin;C:\Windows\System32]=])
sandbox_get_msvc_environment_value(msvc_include "${msvc_environment_text}" INCLUDE)
sandbox_assert_equal("MSVC environment values trim leading whitespace"
  "${msvc_include}" [=[C:\SDK Include\ucrt]=])
sandbox_make_msvc_library_linker_flags(msvc_linker_flags
  [=[C:\VS Tools\lib]=] [=[C:\Windows SDK\lib\x64]=])
sandbox_assert_equal("MSVC linker flags quote normalized library paths"
  "${msvc_linker_flags}"
  [=[ /LIBPATH:"C:/VS Tools/lib" /LIBPATH:"C:/Windows SDK/lib/x64"]=])

sandbox_get_clang_cl_asan_paths(asan_runtime asan_import asan_thunk
  [=[C:\LLVM Root\lib\clang\20]=])
sandbox_assert_equal("ASan runtime path" "${asan_runtime}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic-x86_64.dll]=])
sandbox_assert_equal("ASan import library path" "${asan_import}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic-x86_64.lib]=])
sandbox_assert_equal("ASan thunk path" "${asan_thunk}"
  [=[C:/LLVM Root/lib/clang/20/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib]=])

sandbox_configure_unreal_engine_paths(engine
  [=[C:\Engine Root\UE 5.8]=] UnrealEditor-Win64-DebugGame.exe
  UnrealEditor-Win64-DebugGame-Cmd.exe)
sandbox_assert_equal("Unreal build script path" "${engine_BUILD_SCRIPT}"
  [=[C:/Engine Root/UE 5.8/Engine/Build/BatchFiles/RunUBT.bat]=])
sandbox_assert_equal("Unreal editor command path" "${engine_EDITOR_CMD_EXE}"
  [=[C:/Engine Root/UE 5.8/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe]=])
sandbox_assert_equal("UnrealPak path" "${engine_UNREAL_PAK_EXE}"
  [=[C:/Engine Root/UE 5.8/Engine/Binaries/Win64/UnrealPak.exe]=])

sandbox_get_unreal_target_executable_path(development_test_executable
  [=[C:\Sandbox Root]=] Win64 SandboxCoreTests Development)
sandbox_assert_equal("Development test executable path" "${development_test_executable}"
  [=[C:/Sandbox Root/Binaries/Win64/SandboxCoreTests/SandboxCoreTests.exe]=])
sandbox_get_unreal_target_executable_path(debug_game_test_executable
  [=[C:\Sandbox Root]=] Win64 SandboxCoreTests DebugGame)
sandbox_assert_equal("DebugGame test executable path" "${debug_game_test_executable}"
  [=[C:/Sandbox Root/Binaries/Win64/SandboxCoreTests/SandboxCoreTests-Win64-DebugGame.exe]=])

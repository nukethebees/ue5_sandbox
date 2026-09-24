include("${PROJECT_SOURCE_DIR}/cmake/jobserver_commands.cmake")

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

include("${PROJECT_SOURCE_DIR}/cmake/jobserver_commands.cmake")
set(cmake_test_cli [=[C:\Users\Test User\jobserver.exe]=])
set(cmake_test_worktree [=[C:\worktree two]=])
sandbox_make_jobserver_command(standard_command "${cmake_test_cli}"
  "${cmake_test_worktree}" STANDARD build "Build ordinary work")
sandbox_assert_list_equal("ordinary inner commands do not reacquire machine"
  "${standard_command}" "")
sandbox_make_jobserver_command(benchmark_command "${cmake_test_cli}"
  "${cmake_test_worktree}" BENCHMARK benchmark "Measure benchmark")
sandbox_assert_list_equal("benchmarks explicitly acquire exclusive machine"
  "${benchmark_command}"
  "${cmake_test_cli};run;--name;Measure benchmark;--kind;benchmark;--worktree;${cmake_test_worktree};--exclusive;machine;--")
sandbox_make_unreal_jobserver_resource(resource "C:/Unreal")
string(SHA256 expected_hash "c:/unreal")
sandbox_assert_equal("canonical engine gate" "${resource}" "unreal-build/${expected_hash}")

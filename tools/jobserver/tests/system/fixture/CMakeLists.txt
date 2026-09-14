cmake_minimum_required(VERSION 4.4.2)

if(NOT DEFINED JOBSERVER_CLI OR NOT DEFINED JOBSERVER_WORKTREE)
  message(FATAL_ERROR "JOBSERVER_CLI and JOBSERVER_WORKTREE are required")
endif()

set(CMAKE_CXX_COMPILER_LAUNCHER
  "${JOBSERVER_CLI};run;--name;system-test-compilation;--kind;standard;--worktree;${JOBSERVER_WORKTREE};--shared;machine;--"
)
set(CMAKE_CXX_LINKER_LAUNCHER
  "${JOBSERVER_CLI};run;--name;system-test-link;--kind;standard;--worktree;${JOBSERVER_WORKTREE};--shared;machine;--"
)

project(JobserverSystemFixture LANGUAGES CXX)

add_executable(jobserver-system-fixture)
target_sources(jobserver-system-fixture PRIVATE main.cpp)
target_compile_features(jobserver-system-fixture PRIVATE cxx_std_23)

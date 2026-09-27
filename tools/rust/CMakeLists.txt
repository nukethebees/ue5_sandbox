set(IOJ_RUST_TOOLS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(IOJ_SET_LIVE_CODING_DISABLED_MANIFEST
  "${IOJ_RUST_TOOLS_ROOT}/Cargo.toml")
set(IOJ_SET_LIVE_CODING_DISABLED_TARGET_DIRECTORY
  "${CMAKE_BINARY_DIR}/rust-tools")
set(IOJ_SET_LIVE_CODING_DISABLED_BUILD_OUTPUT
  "${IOJ_SET_LIVE_CODING_DISABLED_TARGET_DIRECTORY}/release/set-live-coding-disabled.exe")

if(NOT DEFINED ENV{LOCALAPPDATA} OR "$ENV{LOCALAPPDATA}" STREQUAL "")
  message(FATAL_ERROR "LOCALAPPDATA is required to locate the canonical developer tools.")
endif()

cmake_path(SET IOJ_DEVELOPER_TOOLS_INSTALL_ROOT NORMALIZE
  "$ENV{LOCALAPPDATA}/NukeTheBees")
cmake_path(APPEND IOJ_DEVELOPER_TOOLS_INSTALL_ROOT bin set-live-coding-disabled.exe
  OUTPUT_VARIABLE IOJ_SET_LIVE_CODING_DISABLED_TOOL)
set(IOJ_SET_LIVE_CODING_DISABLED_TOOL "${IOJ_SET_LIVE_CODING_DISABLED_TOOL}"
  CACHE INTERNAL "Canonical set-live-coding-disabled executable")

sandbox_jobserver_command(rust_build_command STANDARD build "Build set-live-coding-disabled")
sandbox_jobserver_command(rust_test_command STANDARD test "Test set-live-coding-disabled")
add_custom_target(set-live-coding-disabled
  COMMAND ${rust_build_command} cargo build
    --manifest-path "${IOJ_SET_LIVE_CODING_DISABLED_MANIFEST}"
    --package set-live-coding-disabled
    --release
    --target-dir "${IOJ_SET_LIVE_CODING_DISABLED_TARGET_DIRECTORY}"
  WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
  COMMENT "Building Rust tool set-live-coding-disabled"
  VERBATIM
)

add_custom_target(install-set-live-coding-disabled
  COMMAND pwsh -NoProfile -File
    "${IOJ_RUST_TOOLS_ROOT}/install/Install-SetLiveCodingDisabled.ps1"
    -BuiltToolPath "${IOJ_SET_LIVE_CODING_DISABLED_BUILD_OUTPUT}"
    -InstallRoot "${IOJ_DEVELOPER_TOOLS_INSTALL_ROOT}"
  DEPENDS set-live-coding-disabled
  COMMENT "Installing canonical Rust tool set-live-coding-disabled"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(require-set-live-coding-disabled
  COMMAND "${CMAKE_COMMAND}"
    "-DToolPath=${IOJ_SET_LIVE_CODING_DISABLED_TOOL}"
    -P "${IOJ_RUST_TOOLS_ROOT}/cmake/RequireInstalledTool.cmake"
  COMMENT "Checking for canonical set-live-coding-disabled installation"
  VERBATIM
)

add_test(
  NAME Sandbox.RustSetLiveCodingDisabled
  COMMAND ${rust_test_command} cargo test
    --manifest-path "${IOJ_SET_LIVE_CODING_DISABLED_MANIFEST}"
    --package set-live-coding-disabled
)
set_tests_properties(Sandbox.RustSetLiveCodingDisabled PROPERTIES
  LABELS "developer-tool;rust;integration;subprocess"
  WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
)

if(IOJ_BUILD_DEVELOPER_TOOLS)
  set(IOJ_AGENT_TASK_TARGET_DIRECTORY "${CMAKE_BINARY_DIR}/rust-tools")
  sandbox_jobserver_command(agent_task_build_command STANDARD build "Build agent-task")
  sandbox_jobserver_command(agent_task_test_command STANDARD test "Test agent-task")

  add_custom_target(agent-task
    COMMAND ${agent_task_build_command} cargo build
      --manifest-path "${IOJ_RUST_TOOLS_ROOT}/Cargo.toml"
      --package agent-task --release --locked
      --target-dir "${IOJ_AGENT_TASK_TARGET_DIRECTORY}"
    WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
    COMMENT "Building Rust tool agent-task"
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(install-agent-task
    COMMAND ${agent_task_build_command} cargo install
      --path "${IOJ_RUST_TOOLS_ROOT}/crates/agent-task"
      --root "${IOJ_DEVELOPER_TOOLS_INSTALL_ROOT}" --locked --force
      --target-dir "${IOJ_AGENT_TASK_TARGET_DIRECTORY}"
    WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
    COMMENT "Installing agent-task in the per-user developer tools directory"
    USES_TERMINAL
    VERBATIM
  )

  add_test(
    NAME Sandbox.RustAgentTask
    COMMAND ${agent_task_test_command} cargo test
      --manifest-path "${IOJ_RUST_TOOLS_ROOT}/Cargo.toml"
      --package agent-task --locked
      --target-dir "${IOJ_AGENT_TASK_TARGET_DIRECTORY}"
  )
  set_tests_properties(Sandbox.RustAgentTask PROPERTIES
    LABELS "developer-tool;rust;agent-task;integration;subprocess"
    WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
  )
endif()

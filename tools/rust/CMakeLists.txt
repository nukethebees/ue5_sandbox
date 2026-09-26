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

add_custom_target(set-live-coding-disabled
  COMMAND cargo build
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
  COMMAND cargo test
    --manifest-path "${IOJ_SET_LIVE_CODING_DISABLED_MANIFEST}"
    --package set-live-coding-disabled
)
set_tests_properties(Sandbox.RustSetLiveCodingDisabled PROPERTIES
  LABELS "developer-tool;rust;integration;subprocess"
  WORKING_DIRECTORY "${IOJ_RUST_TOOLS_ROOT}"
)

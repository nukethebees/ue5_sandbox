cmake_minimum_required(VERSION 3.25)
project(agent-scheduler-example NONE)

find_program(CARGO cargo REQUIRED)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CODEX_ROOT "${REPO_ROOT}/.local/codex-upstream")
set(EXAMPLE_TARGET "${REPO_ROOT}/.local/scheduler-target")

add_custom_target(scheduler-example
    COMMAND "${CARGO}" build --locked --bins --examples --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
add_custom_target(codex-scheduler
    COMMAND "${CARGO}" build --locked -p codex-cli --bin codex -p codex-windows-sandbox --bin codex-command-runner
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)
add_custom_target(scheduler-unit-tests
    COMMAND "${CARGO}" test --locked --lib --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
add_custom_target(codex-process-tests
    COMMAND "${CARGO}" test --locked -p codex-windows-sandbox --lib
        --target-dir "${EXAMPLE_TARGET}" unified_exec::backends::windows_common::tests
    COMMAND "${CARGO}" test --locked -p codex-utils-pty --lib
        --target-dir "${EXAMPLE_TARGET}" driver_backed_process
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)

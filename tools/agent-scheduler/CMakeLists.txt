cmake_minimum_required(VERSION 3.25)
project(agent-scheduler-example NONE)

find_program(CARGO cargo REQUIRED)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CODEX_ROOT "${REPO_ROOT}/.local/codex-upstream")
set(EXAMPLE_TARGET "${REPO_ROOT}/.local/scheduler-target")
set(CARGO_PROFILE_ARGS)
set(CARGO_PROFILE_DIR debug)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CARGO_PROFILE_ARGS --release)
    set(CARGO_PROFILE_DIR release)
endif()

add_custom_target(scheduler-client
    COMMAND "${CARGO}" build --locked --bin agent-scheduler ${CARGO_PROFILE_ARGS}
        --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)

add_custom_target(scheduler-example
    COMMAND "${CARGO}" build --locked --bins --examples --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
add_custom_target(codex-scheduler
    COMMAND "${CARGO}" build --locked -p codex-cli --bin codex ${CARGO_PROFILE_ARGS}
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)

install(PROGRAMS "${EXAMPLE_TARGET}/${CARGO_PROFILE_DIR}/codex.exe"
    DESTINATION bin RENAME codex-scheduler.exe)
install(PROGRAMS "${EXAMPLE_TARGET}/${CARGO_PROFILE_DIR}/agent-scheduler.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/agent-codex.ps1" DESTINATION bin)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/scheduling.rules"
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-revision.txt" DESTINATION bin)
add_custom_target(scheduler-unit-tests
    COMMAND "${CARGO}" test --locked --lib --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --target-dir "${EXAMPLE_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
add_custom_target(codex-process-tests
    COMMAND "${CARGO}" test --locked -p codex-utils-pty --lib
        --target-dir "${EXAMPLE_TARGET}" driver_backed_process
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)

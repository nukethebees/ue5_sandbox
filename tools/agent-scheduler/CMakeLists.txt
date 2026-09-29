cmake_minimum_required(VERSION 3.25)
project(agent-scheduler NONE)

find_program(CARGO cargo REQUIRED)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CODEX_ROOT "${REPO_ROOT}/.local/codex-upstream")
set(SCHEDULER_TARGET "${REPO_ROOT}/.local/scheduler-target")
set(CARGO_PROFILE_ARGS)
set(CARGO_PROFILE_DIR debug)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CARGO_PROFILE_ARGS --release)
    set(CARGO_PROFILE_DIR release)
elseif(CMAKE_BUILD_TYPE STREQUAL "ReleaseNoLTO")
    set(CARGO_PROFILE_ARGS --config "${CMAKE_CURRENT_SOURCE_DIR}/build-profile.toml"
        --profile release-no-lto)
    set(CARGO_PROFILE_DIR release-no-lto)
endif()

add_custom_target(codex-scheduler
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_codex.py"
        "${CARGO}" build --locked -p codex-cli -p codex-code-mode-host
        --bin codex --bin codex-code-mode-host ${CARGO_PROFILE_ARGS}
        --target-dir "${SCHEDULER_TARGET}"
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)

install(PROGRAMS "${SCHEDULER_TARGET}/${CARGO_PROFILE_DIR}/codex.exe"
    DESTINATION bin RENAME codex-scheduler.exe)
install(PROGRAMS "${SCHEDULER_TARGET}/${CARGO_PROFILE_DIR}/codex-code-mode-host.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/agent-codex.ps1" DESTINATION bin)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/scheduling.rules"
    DESTINATION bin RENAME scheduling.default.rules)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/upstream-revision.txt" DESTINATION bin)
add_custom_target(scheduler-unit-tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_architecture.py"
    COMMAND "${CARGO}" test --locked --lib --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --target-dir "${SCHEDULER_TARGET}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL)
add_custom_target(codex-retry-test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_codex.py"
        "${CARGO}" test --locked -p codex-core --lib sandbox_denied_retry_consumes_one_scheduler_ticket
        --target-dir "${SCHEDULER_TARGET}"
    WORKING_DIRECTORY "${CODEX_ROOT}/codex-rs"
    USES_TERMINAL)

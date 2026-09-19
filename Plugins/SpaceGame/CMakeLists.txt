set(sandbox_architecture_checks
  "${PROJECT_SOURCE_DIR}/tools/bin/ArchitectureChecks.exe")
sandbox_add_staged_csharp_tool_preflight(architecture-checks-preflight
  "ArchitectureChecks.exe" "${sandbox_architecture_checks}")

add_custom_target(check-space-game-layers
  COMMAND "${sandbox_architecture_checks}" --root "${PROJECT_SOURCE_DIR}"
  DEPENDS architecture-checks-preflight
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Checking SpaceGame module boundaries"
  VERBATIM
)
add_test(NAME SpaceGame.Architecture
  COMMAND "${CMAKE_COMMAND}"
    "-DSANDBOX_SOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DSANDBOX_STAGED_TOOL_NAME:STRING=ArchitectureChecks.exe"
    "-DSANDBOX_STAGED_TOOL_PATH:FILEPATH=${sandbox_architecture_checks}"
    -P "${PROJECT_SOURCE_DIR}/cmake/run_space_game_architecture_check.cmake")
set_tests_properties(
  SpaceGame.Architecture
  PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

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
  COMMAND "${sandbox_architecture_checks}" --root "${PROJECT_SOURCE_DIR}")
set_tests_properties(
  SpaceGame.Architecture
  PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

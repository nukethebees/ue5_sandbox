sandbox_add_dotnet_host_tool(
  architecture-checks-host
  SANDBOX_ARCHITECTURE_CHECKS
  "${PROJECT_SOURCE_DIR}/tools/ArchitectureChecks/ArchitectureChecks.csproj"
)

add_custom_target(check-space-game-layers
  COMMAND "${SANDBOX_ARCHITECTURE_CHECKS}" --root "${PROJECT_SOURCE_DIR}"
  DEPENDS architecture-checks-host
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Checking SpaceGame module boundaries"
  VERBATIM
)
add_test(NAME SpaceGame.Architecture
  COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}"
    --target check-space-game-layers)
set_tests_properties(
  SpaceGame.Architecture
  PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

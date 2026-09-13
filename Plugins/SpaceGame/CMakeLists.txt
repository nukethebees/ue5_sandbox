add_custom_target(check-space-game-layers
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Scripts/check_space_game_layers.py"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Checking SpaceGame module boundaries"
  VERBATIM
)
add_test(NAME SpaceGame.Architecture
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Scripts/check_space_game_layers.py")
add_test(NAME SpaceGame.ArchitectureChecker
  COMMAND "${Python3_EXECUTABLE}" -m unittest Scripts.test_space_game_layers)
set_tests_properties(
  SpaceGame.Architecture
  SpaceGame.ArchitectureChecker
  PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

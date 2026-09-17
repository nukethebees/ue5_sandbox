add_custom_target(format-code
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/format-cpp.py" --changed
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/format-cpp.py"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-cmake-presets
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/presets/generate.py" --check
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking generated CMake presets"
  VERBATIM
)
set_property(TARGET check-cmake-presets PROPERTY FOLDER "Sandbox/Checks")

add_test(NAME CMake.Presets
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/presets/generate.py" --check
)
set_tests_properties(CMake.Presets PROPERTIES
  LABELS "unit;all;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

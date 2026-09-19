add_custom_target(format-code
  COMMAND "${PROJECT_SOURCE_DIR}/tools/bin/CodeFormatTools.exe" --changed
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${PROJECT_SOURCE_DIR}/tools/bin/CodeFormatTools.exe"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

add_subdirectory(perf)

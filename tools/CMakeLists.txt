set(sandbox_code_format_tools
  "${PROJECT_SOURCE_DIR}/tools/bin/CodeFormatTools.exe")
sandbox_add_staged_csharp_tool_preflight(code-format-tools-preflight
  "CodeFormatTools.exe" "${sandbox_code_format_tools}")

add_custom_target(format-code
  COMMAND "${sandbox_code_format_tools}" --changed
  DEPENDS code-format-tools-preflight
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${sandbox_code_format_tools}"
  DEPENDS code-format-tools-preflight
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

add_subdirectory(perf)

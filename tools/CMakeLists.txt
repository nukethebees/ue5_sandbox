sandbox_add_dotnet_host_tool(
  code-format-tools-host
  IOJ_CODE_FORMAT_TOOLS
  "${PROJECT_SOURCE_DIR}/tools/CodeFormatTools/CodeFormatTools.csproj"
)

add_custom_target(format-code
  COMMAND "${IOJ_CODE_FORMAT_TOOLS}" --changed
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${IOJ_CODE_FORMAT_TOOLS}"
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
add_test(
  NAME Sandbox.CSharpTools
  COMMAND "${DOTNET_EXECUTABLE}" test "${PROJECT_SOURCE_DIR}/tools/Tools.slnx" -m:1
)
set_tests_properties(Sandbox.CSharpTools PROPERTIES
  LABELS "developer-tool"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_subdirectory(perf)
add_subdirectory(layout_planner)
add_subdirectory(image_lab)

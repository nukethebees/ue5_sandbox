sandbox_add_dotnet_host_tool(
  code-format-tools-host
  SANDBOX_CODE_FORMAT_TOOLS
  "${PROJECT_SOURCE_DIR}/tools/CodeFormatTools/CodeFormatTools.csproj"
)

set(sandbox_benchmark_tools
  "${PROJECT_SOURCE_DIR}/tools/bin/BenchmarkTools.exe")
sandbox_add_staged_csharp_tool_preflight(benchmark-tools-preflight
  "BenchmarkTools.exe" "${sandbox_benchmark_tools}")

add_custom_target(format-code
  COMMAND "${SANDBOX_CODE_FORMAT_TOOLS}" --changed
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${SANDBOX_CODE_FORMAT_TOOLS}"
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

add_subdirectory(perf)

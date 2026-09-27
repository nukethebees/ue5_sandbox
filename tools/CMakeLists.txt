if(NOT IOJ_BUILD_DEVELOPER_TOOLS)
  return()
endif()

sandbox_add_dotnet_host_tool(
  code-format-tools-host
  IOJ_CODE_FORMAT_TOOLS
  "${PROJECT_SOURCE_DIR}/tools/CodeFormatTools/CodeFormatTools.csproj"
)

sandbox_jobserver_command(format_command STANDARD format "Format project sources")
add_custom_target(format-code
  COMMAND ${format_command} "${CMAKE_COMMAND}" -E env "LLVM_ROOT=${LLVM_ROOT}"
    "${IOJ_CODE_FORMAT_TOOLS}" --changed
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND ${format_command} "${CMAKE_COMMAND}" -E env "LLVM_ROOT=${LLVM_ROOT}"
    "${IOJ_CODE_FORMAT_TOOLS}"
  DEPENDS code-format-tools-host
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

include("${PROJECT_SOURCE_DIR}/cmake/csharp_tests.cmake")

add_subdirectory(perf)
add_subdirectory(layout_planner)
add_subdirectory(image_lab)

add_custom_target(developer-tools-build
  DEPENDS csharp-tests-build layout-planner-ui-tests image-lab-tests tracy-benchmark-compare-tests
)
add_custom_target(task-start-build
  DEPENDS native-tests developer-tools-build
)

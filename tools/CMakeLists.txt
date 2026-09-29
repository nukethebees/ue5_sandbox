if(NOT IOJ_BUILD_DEVELOPER_TOOLS)
  return()
endif()

add_custom_target(format-code
  COMMAND "${CMAKE_COMMAND}" -E env "LLVM_ROOT=${LLVM_ROOT}"
    agent-task format --changed
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${CMAKE_COMMAND}" -E env "LLVM_ROOT=${LLVM_ROOT}"
    agent-task format --all
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

include("${PROJECT_SOURCE_DIR}/cmake/csharp_tests.cmake")
add_subdirectory(rust)

add_subdirectory(perf)
add_subdirectory(layout_planner)
add_subdirectory(image_lab)

add_custom_target(developer-tools-build
  DEPENDS csharp-tests-build rust-tool-tests-build layout-planner-ui-tests image-lab-tests tracy-benchmark-compare-tests
)
add_custom_target(task-start-build
  DEPENDS native-tests developer-tools-build
)

if(NOT DEFINED SANDBOX_SOURCE_DIR)
  message(FATAL_ERROR "SANDBOX_SOURCE_DIR is required.")
endif()

include("${SANDBOX_SOURCE_DIR}/cmake/require_staged_csharp_tool.cmake")

execute_process(
  COMMAND "${SANDBOX_STAGED_TOOL_PATH}" --root "${SANDBOX_SOURCE_DIR}"
  WORKING_DIRECTORY "${SANDBOX_SOURCE_DIR}"
  RESULT_VARIABLE architecture_check_result
)
if(NOT architecture_check_result EQUAL 0)
  message(FATAL_ERROR
    "SpaceGame architecture check failed with exit code ${architecture_check_result}.")
endif()

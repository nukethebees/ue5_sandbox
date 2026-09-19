if(NOT DEFINED SANDBOX_STAGED_TOOL_NAME OR
   NOT DEFINED SANDBOX_STAGED_TOOL_PATH)
  message(FATAL_ERROR "Staged C# tool preflight requires a tool name and path.")
endif()

if(NOT EXISTS "${SANDBOX_STAGED_TOOL_PATH}")
  message(FATAL_ERROR
    "${SANDBOX_STAGED_TOOL_NAME} is not available. Run 'ctools' to build and stage the C# developer tools.")
endif()

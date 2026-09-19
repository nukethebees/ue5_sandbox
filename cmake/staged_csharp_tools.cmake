include_guard(GLOBAL)

function(sandbox_add_staged_csharp_tool_preflight target_name tool_name tool_path)
  add_custom_target(${target_name}
    COMMAND "${CMAKE_COMMAND}"
      "-DSANDBOX_STAGED_TOOL_NAME:STRING=${tool_name}"
      "-DSANDBOX_STAGED_TOOL_PATH:FILEPATH=${tool_path}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/require_staged_csharp_tool.cmake"
    COMMENT "Checking staged ${tool_name}"
    VERBATIM
  )
endfunction()

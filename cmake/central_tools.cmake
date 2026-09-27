include_guard(GLOBAL)

# Resolve at invocation time, so configuring bootstrap targets needs no installed tools.
function(sandbox_central_tool_command output_variable tool_name)
  set(${output_variable} "${CMAKE_COMMAND}" "-DToolName=${tool_name}"
    -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_central_tool.cmake" -- PARENT_SCOPE)
endfunction()

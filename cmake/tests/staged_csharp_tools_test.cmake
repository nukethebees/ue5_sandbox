if(NOT DEFINED SANDBOX_SOURCE_DIR)
  message(FATAL_ERROR "SANDBOX_SOURCE_DIR is required.")
endif()

set(preflight_script
  "${SANDBOX_SOURCE_DIR}/cmake/require_staged_csharp_tool.cmake")
set(test_root "${CMAKE_CURRENT_BINARY_DIR}/staged CSharp tools")
set(available_tool "${test_root}/CodeFormatTools.exe")
set(missing_tool "${test_root}/missing/CodeFormatTools.exe")

file(MAKE_DIRECTORY "${test_root}")
file(WRITE "${available_tool}" "")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSANDBOX_STAGED_TOOL_NAME:STRING=CodeFormatTools.exe"
    "-DSANDBOX_STAGED_TOOL_PATH:FILEPATH=${available_tool}"
    -P "${preflight_script}"
  RESULT_VARIABLE available_result
  OUTPUT_VARIABLE available_output
  ERROR_VARIABLE available_error
)
if(NOT available_result EQUAL 0)
  message(FATAL_ERROR
    "The staged-tool preflight rejected an available executable:\n"
    "${available_output}${available_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSANDBOX_STAGED_TOOL_NAME:STRING=CodeFormatTools.exe"
    "-DSANDBOX_STAGED_TOOL_PATH:FILEPATH=${missing_tool}"
    -P "${preflight_script}"
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_output
  ERROR_VARIABLE missing_error
)
if(missing_result EQUAL 0)
  message(FATAL_ERROR "The staged-tool preflight accepted a missing executable.")
endif()

string(CONCAT missing_diagnostic "${missing_output}" "${missing_error}")
if(NOT missing_diagnostic MATCHES "CodeFormatTools\\.exe is not available\\." OR
   NOT missing_diagnostic MATCHES "Run 'ctools'" OR
   NOT missing_diagnostic MATCHES "C# developer tools")
  message(FATAL_ERROR
    "The missing-tool diagnostic did not explain how to recover:\n${missing_diagnostic}")
endif()

file(REMOVE_RECURSE "${test_root}")

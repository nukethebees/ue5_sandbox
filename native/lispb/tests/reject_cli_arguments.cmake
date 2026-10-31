function(expect_rejection diagnostic)
  execute_process(
    COMMAND "${EXECUTABLE}" generate ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  string(FIND "${output}${error}" "${diagnostic}" diagnostic_position)
  if(NOT result STREQUAL "1" OR diagnostic_position EQUAL -1)
    message(FATAL_ERROR
      "Expected rejection '${diagnostic}', got exit ${result}:\n${output}${error}"
    )
  endif()
endfunction()

set(valid_arguments --project "${PROJECT_FILE}" --target sandbox-code)
expect_rejection("Unknown argument: --unknown" ${valid_arguments} --unknown value)
expect_rejection("Missing value after --build-root" ${valid_arguments} --build-root)
expect_rejection("Duplicate argument: --target" ${valid_arguments} --target sandbox-code)
expect_rejection("Cannot open LispB source:" --project missing-project.lispb --target sandbox-code)

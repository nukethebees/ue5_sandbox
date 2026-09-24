execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${REJECT_TARGET}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(result EQUAL 0)
  message(FATAL_ERROR "Invalid packed field compiled successfully")
endif()
if(NOT "${output}${error}" MATCHES "Packed field (requires|must fit)")
  message(FATAL_ERROR "Compilation failed without the expected packed field diagnostic: ${output}${error}")
endif()

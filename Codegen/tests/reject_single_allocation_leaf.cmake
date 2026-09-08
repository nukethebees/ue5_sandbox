execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${REJECT_TARGET}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(result EQUAL 0)
  message(FATAL_ERROR "Unsupported single-allocation leaf compiled successfully")
endif()
if(NOT "${output}${error}" MATCHES "Single-allocation leaf restricted requires")
  message(FATAL_ERROR "Compilation failed without the expected leaf diagnostic: ${output}${error}")
endif()

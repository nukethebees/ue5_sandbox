foreach(argument IN ITEMS BUILD_DIR REJECT_TARGET EXPECTED_DIAGNOSTIC)
  if(NOT DEFINED ${argument} OR "${${argument}}" STREQUAL "")
    message(FATAL_ERROR "Expected compile failure requires ${argument}")
  endif()
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target "${REJECT_TARGET}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(result STREQUAL "0")
  message(FATAL_ERROR "${REJECT_TARGET} compiled successfully; expected rejection")
endif()
if(NOT "${output}${error}" MATCHES "${EXPECTED_DIAGNOSTIC}")
  message(FATAL_ERROR
    "${REJECT_TARGET} failed without the expected diagnostic (${EXPECTED_DIAGNOSTIC}):\n${output}${error}")
endif()

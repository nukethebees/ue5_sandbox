execute_process(COMMAND "${PROBE}" fixture-platform fixture-arch fixture-config
  OUTPUT_FILE "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Exported probe failed: ${result}")
endif()
execute_process(COMMAND "${VERIFY}" verify "${OUTPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Exported probe output did not round-trip: ${result}")
endif()

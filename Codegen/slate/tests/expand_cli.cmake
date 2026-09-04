execute_process(
  COMMAND "${SLATEC}" --manifest "${FIXTURE}/manifest.json" --expand
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)
if(NOT result EQUAL 0 OR NOT error STREQUAL "")
  message(FATAL_ERROR "Expansion failed: ${result}: ${error}")
endif()
if(NOT output MATCHES "^\\(widget-library CompileSmoke::Widgets" OR
   NOT output MATCHES "\n  \\(function empty" OR
   NOT output MATCHES "\\(widget-class CompileSmoke::Host")
  message(FATAL_ERROR "Unexpected expansion stdout: ${output}")
endif()
if(EXISTS "${FIXTURE}/generated")
  message(FATAL_ERROR "Expansion must not create a generated directory beside the fixture")
endif()

foreach(extra IN ITEMS --check --output-root --expand)
  set(arguments "${extra}")
  if(extra STREQUAL "--output-root")
    list(APPEND arguments "${CMAKE_CURRENT_BINARY_DIR}/expand-unused-output")
  endif()
  execute_process(
    COMMAND "${SLATEC}" --manifest "${FIXTURE}/manifest.json" --expand ${arguments}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT result EQUAL 2 OR NOT output STREQUAL "" OR error STREQUAL "")
    message(FATAL_ERROR "Expected argument error for ${extra}: ${result}: ${output}: ${error}")
  endif()
endforeach()

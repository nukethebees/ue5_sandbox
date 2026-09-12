execute_process(
  COMMAND "${SLATEC}" expand
    --project "${PROJECT_FILE}"
    --build-root "${BUILD_ROOT}"
    --target slate-compile-fixture
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

foreach(extra IN ITEMS --unknown --depfile)
  set(arguments "${extra}" "unused")
  execute_process(
    COMMAND "${SLATEC}" expand
      --project "${PROJECT_FILE}"
      --build-root "${BUILD_ROOT}"
      --target slate-compile-fixture
      ${arguments}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT result EQUAL 2 OR NOT output STREQUAL "" OR error STREQUAL "")
    message(FATAL_ERROR "Expected argument error for ${extra}: ${result}: ${output}: ${error}")
  endif()
endforeach()

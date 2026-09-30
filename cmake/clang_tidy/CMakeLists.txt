if(NOT IOJ_ENABLE_CLANG_TIDY)
  return()
endif()

if(NOT IOJ_IS_CLANG_CL)
  message(FATAL_ERROR "IOJ_ENABLE_CLANG_TIDY requires the clang-cl toolchain.")
endif()

ioj_find_llvm_tool(IOJ_CLANG_TIDY_EXECUTABLE clang-tidy)
execute_process(
  COMMAND "${IOJ_CLANG_TIDY_EXECUTABLE}" "-checks=*" -list-checks
  OUTPUT_VARIABLE ioj_tidy_checks
  ERROR_VARIABLE ioj_tidy_error
  RESULT_VARIABLE ioj_tidy_result)
if(NOT ioj_tidy_result EQUAL 0)
  message(FATAL_ERROR "Cannot query clang-tidy checks: ${ioj_tidy_error}")
endif()
if(ioj_tidy_checks MATCHES "(^|[\r\n])[ \t]+ioj-loop-condition-call([\r\n]|$)")
  message(STATUS "clang-tidy has built-in IOJ checks (machine tool; not rebuilt by game workflows)")
  add_test(NAME ClangTidy.LoopConditionCall
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/llvm/clang_tidy/tests/test_loop_condition_call.py"
      --clang-tidy "${IOJ_CLANG_TIDY_EXECUTABLE}")
  set_tests_properties(ClangTidy.LoopConditionCall PROPERTIES
    LABELS "clang-tidy" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  foreach(check IN ITEMS loop-view-construction loop-view-accessor-call no-pair no-tuple)
    if(ioj_tidy_checks MATCHES "(^|[\r\n])[ \t]+ioj-${check}([\r\n]|$)")
      string(REPLACE "-" "_" test_name "${check}")
      add_test(NAME "ClangTidy.${check}"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/llvm/clang_tidy/tests/test_${test_name}.py"
          --clang-tidy "${IOJ_CLANG_TIDY_EXECUTABLE}")
      set_tests_properties("ClangTidy.${check}" PROPERTIES
        LABELS "clang-tidy" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()
  endforeach()
  if(ioj_tidy_checks MATCHES "(^|[\r\n])[ \t]+ioj-no-tuple([\r\n]|$)")
    add_test(NAME ClangTidy.SimulationPolicy
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/llvm/clang_tidy/tests/test_simulation_policy.py"
        --clang-tidy "${IOJ_CLANG_TIDY_EXECUTABLE}")
    set_tests_properties(ClangTidy.SimulationPolicy PROPERTIES
      LABELS "clang-tidy" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  endif()
else()
  message(STATUS "clang-tidy has no IOJ checks; running the standard checks only")
endif()

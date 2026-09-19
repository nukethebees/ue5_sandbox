if(NOT SANDBOX_WITH_CLANG_TIDY)
  return()
endif()

if(NOT SANDBOX_MSVC_FRONTEND OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "SANDBOX_WITH_CLANG_TIDY requires the clang-cl toolchain.")
endif()

find_program(SANDBOX_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
find_program(SANDBOX_RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py REQUIRED)
find_program(SANDBOX_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)

set(SANDBOX_CLANG_TIDY_JOBS 0 CACHE STRING
  "Number of parallel run-clang-tidy workers; 0 uses the detected CPU count")
if(NOT SANDBOX_CLANG_TIDY_JOBS MATCHES "^[0-9]+$")
  message(FATAL_ERROR "SANDBOX_CLANG_TIDY_JOBS must be a non-negative integer.")
endif()

set(SANDBOX_NATIVE_CLANG_TIDY_CHECKS
  modernize-use-nullptr
  modernize-use-override
  readability-redundant-control-flow
  readability-redundant-string-init
  performance-unnecessary-value-param
  performance-move-const-arg
  CACHE INTERNAL "Low-noise clang-tidy checks for first-party native code"
)

function(sandbox_configure_native_clang_tidy)
  file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/native" sandbox_native_source_root)
  string(REPLACE "/" "[/\\\\]" sandbox_native_source_root_pattern
    "${sandbox_native_source_root}")
  set(SANDBOX_NATIVE_CLANG_TIDY_SOURCE_FILTER
    "^${sandbox_native_source_root_pattern}[/\\\\](?!third_party[/\\\\])(?!sbx_mimalloc[/\\\\])(?!s7[/\\\\]lib[/\\\\]src[/\\\\]s7_sandbox\\.cpp$)(?!lispb[/\\\\]kernel[/\\\\]tests[/\\\\]standard_anchor_tests\\.cpp$)(?!simulation[/\\\\]src[/\\\\](?:lasers|fighters)[/\\\\]phase_interface\\.cpp$)(?!.*[/\\\\][^/\\\\]*generated[^/\\\\]*[/\\\\])(?!.*[/\\\\]tests[/\\\\]compile(?:_fixture)?[/\\\\])(?!.*[/\\\\][^/\\\\]*reject[^/\\\\]*\\.cpp$).*\\.cpp$"
    PARENT_SCOPE
  )
endfunction()

function(sandbox_add_native_clang_tidy_target)
  list(JOIN SANDBOX_NATIVE_CLANG_TIDY_CHECKS "," sandbox_native_clang_tidy_checks)
  set(sandbox_native_clang_tidy_log "${CMAKE_BINARY_DIR}/clang-tidy.log")
  add_custom_target(native-clang-tidy
    COMMAND ${standard_activity_command}
      "${SANDBOX_POWERSHELL_EXECUTABLE}" -NoProfile -File
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_clang_tidy.ps1"
      -PythonExecutable "${Python3_EXECUTABLE}"
      -RunClangTidyExecutable "${SANDBOX_RUN_CLANG_TIDY_EXECUTABLE}"
      -ClangTidyExecutable "${SANDBOX_CLANG_TIDY_EXECUTABLE}"
      -LogFile "${sandbox_native_clang_tidy_log}"
      -CompilationDatabase "${CMAKE_BINARY_DIR}"
      -Checks "${sandbox_native_clang_tidy_checks}"
      -Jobs "${SANDBOX_CLANG_TIDY_JOBS}"
      -SourceFilter "${SANDBOX_NATIVE_CLANG_TIDY_SOURCE_FILTER}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Auditing first-party native C++ sources with clang-tidy (log: ${sandbox_native_clang_tidy_log})"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

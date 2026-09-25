if(NOT IOJ_ENABLE_CLANG_TIDY)
  return()
endif()

if(NOT IOJ_IS_CLANG_CL)
  message(FATAL_ERROR "IOJ_ENABLE_CLANG_TIDY requires the clang-cl toolchain.")
endif()

find_program(IOJ_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
find_program(IOJ_RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py REQUIRED)
find_program(IOJ_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)

set(IOJ_CLANG_TIDY_JOBS 0 CACHE STRING
  "Number of parallel run-clang-tidy workers; 0 uses the detected CPU count")
if(NOT IOJ_CLANG_TIDY_JOBS MATCHES "^[0-9]+$")
  message(FATAL_ERROR "IOJ_CLANG_TIDY_JOBS must be a non-negative integer.")
endif()

function(sandbox_configure_native_clang_tidy)
  file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/native" sandbox_native_source_root)
  string(REPLACE "/" "[/\\\\]" sandbox_native_source_root_pattern
    "${sandbox_native_source_root}")
  set(IOJ_NATIVE_CLANG_TIDY_SOURCE_ROOT_PATTERN "${sandbox_native_source_root_pattern}"
    PARENT_SCOPE)
  set(IOJ_NATIVE_CLANG_TIDY_SOURCE_FILTER
    "^${sandbox_native_source_root_pattern}[/\\\\](?!third_party[/\\\\])(?!sbx_mimalloc[/\\\\])(?!s7[/\\\\]lib[/\\\\]src[/\\\\]s7_sandbox\\.cpp$)(?!lispb[/\\\\]kernel[/\\\\]tests[/\\\\]standard_anchor_tests\\.cpp$)(?!simulation[/\\\\]src[/\\\\](?:lasers|fighters)[/\\\\]phase_interface\\.cpp$)(?!.*[/\\\\][^/\\\\]*generated[^/\\\\]*[/\\\\])(?!.*[/\\\\]tests[/\\\\]compile(?:_fixture)?[/\\\\])(?!.*[/\\\\][^/\\\\]*reject[^/\\\\]*\\.cpp$).*\\.cpp$"
    PARENT_SCOPE
  )
endfunction()

function(sandbox_add_clang_tidy_target target_name source_filter log_name description)
  set(sandbox_native_clang_tidy_log "${CMAKE_BINARY_DIR}/${log_name}.log")
  sandbox_jobserver_command(clang_tidy_command STANDARD static-analysis
    "clang-tidy: ${description}")
  add_custom_target(${target_name}
    COMMAND ${clang_tidy_command}
      "${IOJ_POWERSHELL_EXECUTABLE}" -NoProfile -File
      "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_clang_tidy.ps1"
      -PythonExecutable "${Python3_EXECUTABLE}"
      -RunClangTidyExecutable "${IOJ_RUN_CLANG_TIDY_EXECUTABLE}"
      -ClangTidyExecutable "${IOJ_CLANG_TIDY_EXECUTABLE}"
      -LogFile "${sandbox_native_clang_tidy_log}"
      -CompilationDatabase "${CMAKE_BINARY_DIR}"
      -Jobs "${IOJ_CLANG_TIDY_JOBS}"
      -SourceFilter "${source_filter}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Auditing ${description} with clang-tidy (log: ${sandbox_native_clang_tidy_log})"
    USES_TERMINAL
    VERBATIM
  )
endfunction()

function(sandbox_add_native_clang_tidy_scope scope directories)
  set(scope_filter
    "^${IOJ_NATIVE_CLANG_TIDY_SOURCE_ROOT_PATTERN}[/\\\\](?:${directories})[/\\\\]")
  sandbox_add_clang_tidy_target("native-clang-tidy-${scope}"
    "(?=${scope_filter})${IOJ_NATIVE_CLANG_TIDY_SOURCE_FILTER}"
    "clang-tidy-${scope}" "native ${scope} sources")
endfunction()

function(sandbox_add_native_clang_tidy_target)
  sandbox_add_clang_tidy_target(native-clang-tidy
    "${IOJ_NATIVE_CLANG_TIDY_SOURCE_FILTER}" clang-tidy "first-party native sources")
  sandbox_add_native_clang_tidy_scope(core "core|profiling")
  sandbox_add_native_clang_tidy_scope(simulation "simulation|simulation_benchmark")
  sandbox_add_native_clang_tidy_scope(layout "layout")
  sandbox_add_native_clang_tidy_scope(lispb "lispb")
  sandbox_add_native_clang_tidy_scope(memory "memory")
  sandbox_add_native_clang_tidy_scope(level-authoring "level_authoring")
  sandbox_add_native_clang_tidy_scope(s7 "s7")
  sandbox_add_native_clang_tidy_scope(image "image")
  sandbox_add_native_clang_tidy_scope(mesh-gen "mesh_gen")
endfunction()

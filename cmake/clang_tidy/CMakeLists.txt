if(NOT IOJ_ENABLE_CLANG_TIDY)
  return()
endif()

if(NOT IOJ_IS_CLANG_CL)
  message(FATAL_ERROR "IOJ_ENABLE_CLANG_TIDY requires the clang-cl toolchain.")
endif()

ioj_find_llvm_tool(IOJ_CLANG_TIDY_EXECUTABLE clang-tidy)
ioj_find_llvm_tool(IOJ_RUN_CLANG_TIDY_EXECUTABLE run-clang-tidy)
find_program(IOJ_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
option(IOJ_CLANG_TIDY_USE_DLL "Build/load the DLL baseline instead of using built-in IOJ checks" OFF)
if(IOJ_CLANG_TIDY_USE_DLL)
  add_subdirectory(plugin)
else()
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
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/plugin/test_loop_condition_call.py"
        --clang-tidy "${IOJ_CLANG_TIDY_EXECUTABLE}"
        --source-dir "${PROJECT_SOURCE_DIR}")
    set_tests_properties(ClangTidy.LoopConditionCall PROPERTIES LABELS "clang-tidy")
  else()
    message(STATUS "clang-tidy has no IOJ checks; running the standard checks only")
  endif()
endif()

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
  set(ioj_clang_tidy_plugin_arguments)
  if(IOJ_CLANG_TIDY_USE_DLL)
    list(APPEND ioj_clang_tidy_plugin_arguments -Plugin "$<TARGET_FILE:ioj-tidy-module>")
  endif()
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
      ${ioj_clang_tidy_plugin_arguments}
      -LogFile "${sandbox_native_clang_tidy_log}"
      -CompilationDatabase "${CMAKE_BINARY_DIR}"
      -Jobs "${IOJ_CLANG_TIDY_JOBS}"
      -SourceFilter "${source_filter}"
    # run-clang-tidy's preliminary list-checks invocation reads the working
    # directory's config; per-file nested policies still apply during analysis.
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/native"
    COMMENT "Auditing ${description} with clang-tidy (log: ${sandbox_native_clang_tidy_log})"
    USES_TERMINAL
    VERBATIM
  )
  if(IOJ_CLANG_TIDY_USE_DLL)
    add_dependencies(${target_name} ioj-tidy-module)
  endif()
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

  foreach(tidy_target IN ITEMS native-clang-tidy native-clang-tidy-lispb)
    add_dependencies(${tidy_target}
      generate-native-soa-fixture
      kernel-native-generated-sources)
  endforeach()
endfunction()

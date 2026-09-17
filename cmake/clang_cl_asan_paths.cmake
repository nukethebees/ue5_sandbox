include_guard(GLOBAL)

function(sandbox_get_clang_cl_asan_paths runtime_output import_library_output thunk_output resource_directory)
  cmake_path(APPEND resource_directory lib windows
    OUTPUT_VARIABLE library_directory)
  cmake_path(APPEND library_directory clang_rt.asan_dynamic-x86_64.dll
    OUTPUT_VARIABLE runtime)
  cmake_path(APPEND library_directory clang_rt.asan_dynamic-x86_64.lib
    OUTPUT_VARIABLE import_library)
  cmake_path(APPEND library_directory clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
    OUTPUT_VARIABLE runtime_thunk)
  set(${runtime_output} "${runtime}" PARENT_SCOPE)
  set(${import_library_output} "${import_library}" PARENT_SCOPE)
  set(${thunk_output} "${runtime_thunk}" PARENT_SCOPE)
endfunction()

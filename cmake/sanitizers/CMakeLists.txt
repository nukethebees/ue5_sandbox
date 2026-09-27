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

option(IOJ_ASAN_WORKAROUND_LLVM_215376
  "Work around LLVM #215376 by disabling Windows ASAN stack-use-after-return instrumentation"
  ON)

add_library(ioj_asan INTERFACE)
add_library(ioj::asan ALIAS ioj_asan)

if(NOT IOJ_ENABLE_ASAN)
  return()
endif()

if(IOJ_WITH_UNREAL)
  message(FATAL_ERROR
    "IOJ_ENABLE_ASAN is supported only for native-only configurations.")
endif()

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR
    "IOJ_ENABLE_ASAN is supported only for Windows x64 configurations.")
endif()

if(NOT IOJ_IS_CLANG_CL)
  message(FATAL_ERROR "IOJ_ENABLE_ASAN requires the Windows clang-cl toolchain.")
endif()

target_compile_options(ioj_asan INTERFACE
  /fsanitize=address
  /clang:-fno-omit-frame-pointer
  /RTC-
)

if(IOJ_ASAN_WORKAROUND_LLVM_215376)
  # Upstream workaround, not project ASAN policy: Windows catch-handler rethrows
  # corrupt the stack with runtime/always use-after-return instrumentation.
  # https://github.com/llvm/llvm-project/issues/215376
  target_compile_options(ioj_asan INTERFACE /clang:-fsanitize-address-use-after-return=never)
  message(STATUS "ASAN: LLVM #215376 workaround active (stack-use-after-return disabled)")
endif()

target_link_options(ioj_asan INTERFACE
  /INCREMENTAL:NO
)

execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
  OUTPUT_VARIABLE clang_resource_directory
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE resource_directory_result
)

if(NOT resource_directory_result EQUAL 0 OR
    clang_resource_directory STREQUAL "")
  message(FATAL_ERROR
    "Unable to discover the clang-cl resource directory for AddressSanitizer.")
endif()

sandbox_get_clang_cl_asan_paths(asan_runtime asan_import_library asan_runtime_thunk
  "${clang_resource_directory}")
foreach(asan_file IN ITEMS
    "${asan_runtime}"
    "${asan_import_library}"
    "${asan_runtime_thunk}")
  if(NOT EXISTS "${asan_file}")
    message(FATAL_ERROR
      "A required clang-cl AddressSanitizer file was not found at '${asan_file}'.")
  endif()
endforeach()

target_link_libraries(ioj_asan INTERFACE "${asan_import_library}")
target_link_options(ioj_asan INTERFACE
  # CMake invokes the linker directly, so clang-cl cannot add its runtime thunk.
  "/wholearchive:${asan_runtime_thunk}"
  /INFERASANLIBS:NO
)

set(runtime_output_directory "${CMAKE_BINARY_DIR}/bin")
set(staged_runtime "${runtime_output_directory}/clang_rt.asan_dynamic-x86_64.dll")
add_custom_command(
  OUTPUT "${staged_runtime}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${runtime_output_directory}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${asan_runtime}" "${staged_runtime}"
  DEPENDS "${asan_runtime}"
  COMMENT "Staging the clang-cl AddressSanitizer runtime"
  VERBATIM
)
add_custom_target(stage-asan-runtime DEPENDS "${staged_runtime}")
add_dependencies(ioj_asan stage-asan-runtime)

include_guard(GLOBAL)

function(sandbox_enable_address_sanitizer)
  if(SANDBOX_WITH_UNREAL)
    message(FATAL_ERROR
      "SANDBOX_WITH_ASAN is supported only for native-only configurations.")
  endif()
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
      "SANDBOX_WITH_ASAN is supported only for Windows x64 configurations.")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
     NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR
      "SANDBOX_WITH_ASAN requires the Windows clang-cl toolchain.")
  endif()

  add_compile_options(
    /fsanitize=address
    /clang:-fno-omit-frame-pointer
    /RTC-
  )
  add_link_options(/INCREMENTAL:NO)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL
    "Disable C++ module dependency scanning for AddressSanitizer builds" FORCE)

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

  cmake_path(APPEND clang_resource_directory lib windows
    OUTPUT_VARIABLE asan_library_directory)
  cmake_path(APPEND asan_library_directory clang_rt.asan_dynamic-x86_64.dll
    OUTPUT_VARIABLE asan_runtime)
  cmake_path(APPEND asan_library_directory clang_rt.asan_dynamic-x86_64.lib
    OUTPUT_VARIABLE asan_import_library)
  cmake_path(APPEND asan_library_directory
    clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
    OUTPUT_VARIABLE asan_runtime_thunk)
  foreach(asan_file IN ITEMS
      "${asan_runtime}"
      "${asan_import_library}"
      "${asan_runtime_thunk}")
    if(NOT EXISTS "${asan_file}")
      message(FATAL_ERROR
        "A required clang-cl AddressSanitizer file was not found at '${asan_file}'.")
    endif()
  endforeach()

  add_link_options(
    "${asan_import_library}"
    # CMake invokes lld-link directly, so clang-cl cannot add its runtime thunk.
    "/wholearchive:${asan_runtime_thunk}"
    /INFERASANLIBS:NO
  )

  set(runtime_output_directory "${CMAKE_BINARY_DIR}/bin")
  file(MAKE_DIRECTORY "${runtime_output_directory}")
  configure_file(
    "${asan_runtime}"
    "${runtime_output_directory}/clang_rt.asan_dynamic-x86_64.dll"
    COPYONLY
  )
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${runtime_output_directory}" PARENT_SCOPE)
endfunction()

set(s7_output_dir "${IOJ_NATIVE_OUTPUT_ROOT}/S7/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}")
add_library(sandbox-s7 STATIC)
target_sources(sandbox-s7
  PRIVATE
    src/interpreter.cpp
    src/ast_materialization.cpp
    src/s7_ownership.cpp
    src/sandbox_policy.cpp
    src/script_bindings.cpp
    src/platform/windows/file_handle.cpp
    src/platform/windows/utf_encoding.cpp
    src/platform/windows/canonical_path.cpp
    src/platform/windows/script_files.cpp
    src/script_loader.cpp
)
target_include_directories(sandbox-s7
  PUBLIC
    include
)

add_library(sandbox-s7-warning-exceptions INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sandbox-s7-warning-exceptions
    INTERFACE
      /wd5039 # The third-party s7 C callback typedefs cannot express noexcept.
  )
endif()
target_link_libraries(sandbox-s7
  PUBLIC
    sandbox::native_defaults
    native-s7-data
  PRIVATE
    sandbox-s7-third-party
    sandbox-s7-warning-exceptions
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
ioj_enable_warnings(sandbox-s7)
ioj_apply_native_abi(sandbox-s7)

set_target_properties(sandbox-s7 PROPERTIES
  OUTPUT_NAME sandbox_s7
)
if(IOJ_WITH_UNREAL)
  foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
    file(MAKE_DIRECTORY
      "${s7_output_dir}/${configuration}"
    )
  endforeach()

  set_target_properties(sandbox-s7 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${s7_output_dir}/${UE_CONFIGURATION}"
  )
endif()

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "sandbox-s7 requires an MSVC-compatible ABI on Windows.")
endif()

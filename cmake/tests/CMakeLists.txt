include("${CMAKE_CURRENT_LIST_DIR}/assertions.cmake")

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_test(NAME CMake.CompilerCache
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/tests/test_compiler_cache.py"
      --source-dir "${PROJECT_SOURCE_DIR}"
      --cmake "${CMAKE_COMMAND}"
      --compiler "${CMAKE_CXX_COMPILER}"
      --archiver "${CMAKE_AR}"
  )
  set_tests_properties(CMake.CompilerCache PROPERTIES
    LABELS "integration;subprocess;all;cmake;compiler-cache"
    TIMEOUT 120
  )
endif()

add_subdirectory(unreal_paths)
add_subdirectory(unreal_arguments)
add_subdirectory(msvc_environment)

add_test(NAME CMake.Presets
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/presets/generate.py" --check
)
set_tests_properties(CMake.Presets PROPERTIES
  LABELS "unit;all;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_test(NAME CMake.NativeWorkflow
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/presets/test_native_workflow.py"
    --source-dir "${PROJECT_SOURCE_DIR}"
    --cmake "${CMAKE_COMMAND}"
    "--llvm-root=${LLVM_ROOT}"
)
set_tests_properties(CMake.NativeWorkflow PROPERTIES
  LABELS "integration;subprocess;all;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_test(NAME CMake.RustHostTools
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/tests/test_rust_host_tools.py"
    --source-dir "${PROJECT_SOURCE_DIR}"
    --cmake "${CMAKE_COMMAND}"
)
set_tests_properties(CMake.RustHostTools PROPERTIES
  LABELS "integration;subprocess;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

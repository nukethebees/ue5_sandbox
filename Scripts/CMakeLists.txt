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

add_test(NAME PowerShell.Navigation
  COMMAND pwsh -NoProfile -File "${PROJECT_SOURCE_DIR}/PowerShell/tests/Test-Navigation.ps1")
set_tests_properties(PowerShell.Navigation PROPERTIES LABELS "developer-tool;scripts;integration;subprocess")

add_custom_target(check-cmake-presets
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/presets/generate.py" --check
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking generated CMake presets"
  VERBATIM
)
set_property(TARGET check-cmake-presets PROPERTY FOLDER "Sandbox/Checks")

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
)
set_tests_properties(CMake.NativeWorkflow PROPERTIES
  LABELS "integration;subprocess;all;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_test(NAME Scripts.BenchmarkToolsBootstrap
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Scripts/test_benchmark_tools_bootstrap.py"
    --source-dir "${PROJECT_SOURCE_DIR}"
    --powershell powershell
)
set_tests_properties(Scripts.BenchmarkToolsBootstrap PROPERTIES
  LABELS "integration;subprocess;all;scripts"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_test(NAME CMake.DotnetHostTools
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/tests/test_dotnet_host_tools.py"
    --source-dir "${PROJECT_SOURCE_DIR}"
    --cmake "${CMAKE_COMMAND}"
)
set_tests_properties(CMake.DotnetHostTools PROPERTIES
  LABELS "integration;subprocess;cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

add_test(NAME CMake.CSharpTests
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/cmake/test_csharp_tests.py")
set_tests_properties(CMake.CSharpTests PROPERTIES
  LABELS "cmake;integration;subprocess"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

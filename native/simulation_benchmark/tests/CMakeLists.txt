add_executable(native-simulation-benchmark-tests)
target_sources(native-simulation-benchmark-tests PRIVATE simulation_benchmark_tests.cpp)
target_compile_features(native-simulation-benchmark-tests PRIVATE cxx_std_23)
target_link_libraries(native-simulation-benchmark-tests PRIVATE
  native-simulation-benchmark-lib
  GTest::gtest_main
  sandbox::warnings_as_errors
)
target_compile_definitions(native-simulation-benchmark-tests PRIVATE
  SANDBOX_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
configure_sandbox_s7_abi(native-simulation-benchmark-tests)

add_test(NAME native-simulation-benchmark-tests COMMAND native-simulation-benchmark-tests)
set_tests_properties(native-simulation-benchmark-tests PROPERTIES
  LABELS "unit;all;native;native-simulation-benchmark"
)

if(SANDBOX_FRAME_MEMORY_LEVEL_BENCHMARK)
  add_test(NAME native-frame-memory-level-benchmark
    COMMAND powershell -NoProfile -File
      "${PROJECT_SOURCE_DIR}/Scripts/run-frame-memory-level-benchmark.ps1"
      -Seconds 20
      -SkipBuild
  )
  set_tests_properties(native-frame-memory-level-benchmark PROPERTIES
    LABELS "frame-memory-level-benchmark"
    TIMEOUT 600
  )
endif()

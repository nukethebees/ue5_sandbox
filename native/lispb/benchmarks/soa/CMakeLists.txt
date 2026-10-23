add_executable(native-soa-benchmarks)
target_sources(native-soa-benchmarks
  PRIVATE
    production_benchmarks.cpp
    "${PROJECT_SOURCE_DIR}/native/simulation/src/lasers/laser_frame_output.cpp"
)
set_target_properties(native-soa-benchmarks PROPERTIES UNITY_BUILD OFF)
ioj_enable_warnings(native-soa-benchmarks)
target_include_directories(native-soa-benchmarks
  PRIVATE
    "${PROJECT_SOURCE_DIR}/native/simulation/include"
    "${PROJECT_SOURCE_DIR}/native/core/include"
)
target_link_libraries(native-soa-benchmarks
  PRIVATE
    sandbox::native_defaults
    native-levels
    benchmark::benchmark_main
    ioj::arch_avx2
    ioj::debug_symbols
    ioj::fp_precise
)
add_test(NAME native-soa-dry-run
  COMMAND native-soa-benchmarks --benchmark_dry_run
    "--benchmark_filter=^Production/"
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-dry-run.json"
    --benchmark_out_format=json
)
set_tests_properties(native-soa-dry-run PROPERTIES LABELS "native-soa")

add_custom_target(native-soa-production-report
  COMMAND "$<TARGET_FILE:native-soa-benchmarks>"
    "--benchmark_filter=Production/" --benchmark_repetitions=7
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.1s
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-production-results.json"
    --benchmark_out_format=json
  DEPENDS native-soa-benchmarks
  USES_TERMINAL
  VERBATIM
)

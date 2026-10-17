add_executable(native-core-benchmarks)
ioj_stage_tbb_runtime(native-core-benchmarks)
target_sources(native-core-benchmarks
  PRIVATE
    projectile_intercept_benchmarks.cpp
    vector_lerp_benchmarks.cpp
    sandbox_ismc_packing_benchmarks.cpp
)
target_link_libraries(native-core-benchmarks
  PRIVATE
    native-core
    benchmark::benchmark
    benchmark::benchmark_main
    sandbox::native_defaults
)
ioj_enable_warnings(native-core-benchmarks)

include("${PROJECT_SOURCE_DIR}/cmake/benchmark_report.cmake")
set(ismc_report_dir "${PROJECT_SOURCE_DIR}/.local/benchmarks/ismc")
ioj_add_benchmark_report(native-core-ismc-benchmark-report native-core-benchmarks
  "^ismc/[^/]+/(scalar|production)/"
  "${ismc_report_dir}/packing-results.json" 5 0.05s
  "Comparing scalar and production Highway packing"
)
ioj_add_benchmark_report(native-core-vector-lerp-benchmark-report native-core-benchmarks
  "^vector_lerp/"
  "${PROJECT_SOURCE_DIR}/out/benchmarks/native-core/vector-lerp-results.json" 7 0.05s
  "Benchmarking fused native 3D lerp against three 1D lerps"
)

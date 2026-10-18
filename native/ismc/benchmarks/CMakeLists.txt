add_executable(native-ismc-benchmarks)
target_sources(native-ismc-benchmarks
  PRIVATE
    packing_benchmarks.cpp
)
target_link_libraries(native-ismc-benchmarks
  PRIVATE
    ioj::ismc
    benchmark::benchmark
    benchmark::benchmark_main
)
ioj_enable_warnings(native-ismc-benchmarks)
include("${PROJECT_SOURCE_DIR}/cmake/benchmark_report.cmake")
ioj_add_benchmark_report(native-ismc-benchmark-report native-ismc-benchmarks
  "^ismc/[^/]+/(scalar|production)/"
  "${PROJECT_SOURCE_DIR}/.local/benchmarks/ismc/packing-results.json" 5 0.05s
  "Comparing scalar and production Highway packing"
)

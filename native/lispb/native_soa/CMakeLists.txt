add_custom_target(check-generated-native-soa-fixture
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
  DEPENDS lispb
  VERBATIM
)
add_dependencies(check-generated-code check-generated-native-soa-fixture)

add_library(native-soa INTERFACE)
target_include_directories(native-soa
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/native/core/include"
)
target_sources(native-soa
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/native_soa/types.hpp"
)
target_link_libraries(native-soa
  INTERFACE
    sandbox::native_defaults
)

add_executable(native-soa-tests)
target_sources(native-soa-tests
  PRIVATE
    tests.cpp
    src/types.cpp
)
ioj_enable_warnings(native-soa-tests)
target_link_libraries(native-soa-tests PRIVATE native-soa GTest::gtest_main)
add_test(NAME native-soa-tests COMMAND native-soa-tests)
set_tests_properties(native-soa-tests PROPERTIES LABELS "unit;all;native;native-soa")

add_executable(native-soa-mimalloc-smoke mimalloc_smoke.cpp)
ioj_enable_warnings(native-soa-mimalloc-smoke)
target_link_libraries(native-soa-mimalloc-smoke
  PRIVATE
    native-soa
    GTest::gtest_main
    sandbox::mimalloc
    ioj::arch_avx2
    ioj::debug_symbols
    ioj::fp_precise
)
add_test(NAME native-soa-mimalloc-smoke COMMAND native-soa-mimalloc-smoke)
set_tests_properties(native-soa-mimalloc-smoke PROPERTIES LABELS "unit;all;native;native-soa")

add_executable(native-soa-benchmarks)
target_sources(native-soa-benchmarks
  PRIVATE
    production_benchmarks.cpp
    "${PROJECT_SOURCE_DIR}/native/simulation/src/lasers/laser_frame_output.cpp"
)
set_target_properties(native-soa-benchmarks PROPERTIES UNITY_BUILD OFF)
ioj_enable_warnings(native-soa-benchmarks)
target_include_directories(native-soa-benchmarks PRIVATE
  "${PROJECT_SOURCE_DIR}/native/simulation/include"
  "${PROJECT_SOURCE_DIR}/native/core/include")
target_link_libraries(native-soa-benchmarks
  PRIVATE
    sandbox::native_defaults

    native-levels
    benchmark::benchmark_main
    ioj::arch_avx2
    ioj::debug_symbols
    ioj::fp_precise
)
add_test(NAME native-soa-dry-run COMMAND native-soa-benchmarks --benchmark_dry_run
  "--benchmark_filter=^Production/"
  "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-dry-run.json" --benchmark_out_format=json)
set_tests_properties(native-soa-dry-run PROPERTIES LABELS "native-soa")

add_custom_target(native-soa-production-report
  COMMAND "$<TARGET_FILE:native-soa-benchmarks>"
    "--benchmark_filter=Production/" --benchmark_repetitions=7
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.1s
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-production-results.json"
    --benchmark_out_format=json
  DEPENDS native-soa-benchmarks USES_TERMINAL VERBATIM)

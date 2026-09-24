set(NATIVE_SOA_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${NATIVE_SOA_GENERATED}/native_soa_types.h"
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
    --depfile "${NATIVE_SOA_GENERATED}/fixture.d"
  DEPENDS lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${CMAKE_CURRENT_SOURCE_DIR}/types.lispb"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixture.lispb"
  DEPFILE "${NATIVE_SOA_GENERATED}/fixture.d"
  VERBATIM)
add_custom_target(generate-native-soa-fixture
  DEPENDS "${NATIVE_SOA_GENERATED}/native_soa_types.h")
add_custom_target(check-generated-native-soa-fixture
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
  DEPENDS generate-native-soa-fixture
  VERBATIM)
add_dependencies(check-generated-code check-generated-native-soa-fixture)
add_library(native-soa INTERFACE)
target_compile_features(native-soa INTERFACE cxx_std_23)
target_include_directories(native-soa INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${NATIVE_SOA_GENERATED}")
target_include_directories(native-soa INTERFACE "${PROJECT_SOURCE_DIR}/native/core/include")
target_sources(native-soa INTERFACE "${NATIVE_SOA_GENERATED}/native_soa_types.h")
target_link_libraries(native-soa INTERFACE sandbox::handmade_math sandbox::native_defaults)

add_executable(native-soa-tests tests.cpp)
target_link_libraries(native-soa-tests PRIVATE native-soa GTest::gtest_main)
add_test(NAME native-soa-tests COMMAND native-soa-tests)
set_tests_properties(native-soa-tests PROPERTIES LABELS "unit;all;native;native-soa")

add_executable(native-soa-tests-mimalloc tests.cpp)
target_compile_definitions(native-soa-tests-mimalloc PRIVATE NATIVE_SOA_MIMALLOC=1)
target_link_libraries(native-soa-tests-mimalloc PRIVATE native-soa GTest::gtest_main sandbox::mimalloc)
target_link_libraries(native-soa-tests-mimalloc PRIVATE
  sandbox::arch_avx2
  sandbox::debug_symbols
  sandbox::fp_precise
)
add_test(NAME native-soa-tests-mimalloc COMMAND native-soa-tests-mimalloc)
set_tests_properties(native-soa-tests-mimalloc PROPERTIES LABELS "unit;all;native;native-soa")

if(IOJ_NATIVE_SOA_BENCHMARKS)
add_executable(native-soa-benchmarks production_benchmarks.cpp)
target_sources(native-soa-benchmarks PRIVATE
  "${PROJECT_SOURCE_DIR}/native/simulation/src/laser_frame_output.cpp")
set_target_properties(native-soa-benchmarks PROPERTIES UNITY_BUILD OFF)
target_link_libraries(native-soa-benchmarks PRIVATE
  sandbox::native_defaults sandbox::handmade_math benchmark::benchmark_main)
target_include_directories(native-soa-benchmarks PRIVATE
  "${PROJECT_SOURCE_DIR}/native/simulation/include"
  "${PROJECT_SOURCE_DIR}/native/core/include")
target_link_libraries(native-soa-benchmarks PRIVATE
  sandbox::arch_avx2
  sandbox::debug_symbols
  sandbox::fp_precise
)
add_test(NAME native-soa-dry-run COMMAND native-soa-benchmarks --benchmark_dry_run
  "--benchmark_filter=^Production/"
  "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-dry-run.json" --benchmark_out_format=json)
set_tests_properties(native-soa-dry-run PROPERTIES LABELS "native-soa")

sandbox_jobserver_command(native_soa_production_report_command BENCHMARK benchmark
  "Native SoA production benchmark")
add_custom_target(native-soa-production-report
  COMMAND ${native_soa_production_report_command} native-soa-benchmarks
    "--benchmark_filter=Production/" --benchmark_repetitions=7
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.1s
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-production-results.json"
    --benchmark_out_format=json
  DEPENDS native-soa-benchmarks USES_TERMINAL VERBATIM)

sandbox_jobserver_command(native_soa_candidate_report_command BENCHMARK benchmark
  "Native SoA candidate benchmark")
add_custom_target(native-soa-candidate-report
  COMMAND ${native_soa_candidate_report_command} native-soa-benchmarks
    "--benchmark_filter=Production/(Spinner|LaserHit)/" --benchmark_repetitions=7
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.1s
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-candidate-results.json"
    --benchmark_out_format=json
  DEPENDS native-soa-benchmarks USES_TERMINAL VERBATIM)

sandbox_jobserver_command(native_soa_laser_hit_report_command BENCHMARK benchmark
  "Native SoA laser-hit append benchmark")
add_custom_target(native-soa-laser-hit-append-report
  COMMAND ${native_soa_laser_hit_report_command} native-soa-benchmarks
    "--benchmark_filter=Production/LaserHit/" --benchmark_repetitions=9
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.3s
    "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-laser-hit-append-results.json"
    --benchmark_out_format=json
  DEPENDS native-soa-benchmarks USES_TERMINAL VERBATIM)

endif()

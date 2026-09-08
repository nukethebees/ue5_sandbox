find_package(benchmark CONFIG REQUIRED)
find_program(UV_EXECUTABLE NAMES uv REQUIRED)

add_executable(native-soa-generate generate.cpp)
target_link_libraries(native-soa-generate PRIVATE codegen-lib)
set(NATIVE_SOA_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${NATIVE_SOA_GENERATED}/native_soa_types.h"
  COMMAND native-soa-generate "${PROJECT_SOURCE_DIR}" "${NATIVE_SOA_GENERATED}"
  DEPENDS native-soa-generate
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "${PROJECT_SOURCE_DIR}/Codegen/manifests/single_allocation_experiment.json"
    "${PROJECT_SOURCE_DIR}/Codegen/manifests/types.json"
  VERBATIM)
add_library(native-soa INTERFACE)
target_compile_features(native-soa INTERFACE cxx_std_23)
target_include_directories(native-soa INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${NATIVE_SOA_GENERATED}")
target_include_directories(native-soa INTERFACE "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SbxCoreExperiments/Public")
target_sources(native-soa INTERFACE "${NATIVE_SOA_GENERATED}/native_soa_types.h")

add_executable(native-soa-tests tests.cpp)
target_link_libraries(native-soa-tests PRIVATE native-soa GTest::gtest_main)
add_test(NAME native-soa-tests COMMAND native-soa-tests)
set_tests_properties(native-soa-tests PROPERTIES LABELS "native-soa")

add_executable(native-soa-benchmarks benchmarks.cpp)
target_link_libraries(native-soa-benchmarks PRIVATE native-soa benchmark::benchmark_main)
if(MSVC)
  target_compile_options(native-soa-tests PRIVATE /W4 /WX)
  target_compile_options(native-soa-benchmarks PRIVATE /W4 /WX /fp:precise /arch:AVX2 /Zi)
  target_link_options(native-soa-benchmarks PRIVATE /DEBUG)
else()
  target_compile_options(native-soa-tests PRIVATE -Wall -Wextra -Werror)
  target_compile_options(native-soa-benchmarks PRIVATE -Wall -Wextra -Werror -mavx2 -ffp-contract=off)
  target_compile_options(native-soa-benchmarks PRIVATE -g)
  if(WIN32)
    target_compile_options(native-soa-benchmarks PRIVATE -gcodeview)
    target_link_options(native-soa-benchmarks PRIVATE -Xlinker /DEBUG)
  endif()
endif()
add_test(NAME native-soa-dry-run COMMAND native-soa-benchmarks --benchmark_dry_run
  "--benchmark_out=${CMAKE_BINARY_DIR}/native-soa-dry-run.json" --benchmark_out_format=json)
set_tests_properties(native-soa-dry-run PROPERTIES LABELS "native-soa" FIXTURES_SETUP native-soa-json)
add_test(NAME native-soa-plot-smoke COMMAND "${UV_EXECUTABLE}" run "${PROJECT_SOURCE_DIR}/Scripts/plot-native-soa-benchmarks.py"
  --input "${CMAKE_BINARY_DIR}/native-soa-dry-run.json" --output-dir "${CMAKE_BINARY_DIR}/native-soa-dry-run-plots")
set_tests_properties(native-soa-plot-smoke PROPERTIES LABELS "native-soa" FIXTURES_REQUIRED native-soa-json)

set(NATIVE_SOA_RESULTS "${CMAKE_BINARY_DIR}/native-soa-results.json")
add_custom_target(native-soa-reserve-report
  COMMAND native-soa-benchmarks --benchmark_filter=/reserve/ --benchmark_repetitions=9
    --benchmark_enable_random_interleaving=true --benchmark_min_time=0.1s
    "--benchmark_out=${NATIVE_SOA_RESULTS}" --benchmark_out_format=json
  COMMAND "${UV_EXECUTABLE}" run "${PROJECT_SOURCE_DIR}/Scripts/plot-native-soa-benchmarks.py"
    --input "${NATIVE_SOA_RESULTS}" --output-dir "${CMAKE_BINARY_DIR}/native-soa-plots"
  DEPENDS native-soa-benchmarks USES_TERMINAL VERBATIM)

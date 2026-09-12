find_package(benchmark CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_program(UV_EXECUTABLE NAMES uv REQUIRED)

add_executable(native-soa-generate generate.cpp)
target_link_libraries(native-soa-generate PRIVATE codegen-lib sandbox::warnings_as_errors)
set(NATIVE_SOA_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${NATIVE_SOA_GENERATED}/native_soa_types.h"
  COMMAND native-soa-generate "${PROJECT_SOURCE_DIR}" "${NATIVE_SOA_GENERATED}"
  DEPENDS native-soa-generate
    "${PROJECT_SOURCE_DIR}/lispb/schema/single_allocation_experiment.lispb"
    "${PROJECT_SOURCE_DIR}/lispb/schema/types.lispb"
  VERBATIM)
add_library(native-soa INTERFACE)
target_compile_features(native-soa INTERFACE cxx_std_23)
target_include_directories(native-soa INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${NATIVE_SOA_GENERATED}")
target_include_directories(native-soa INTERFACE "${PROJECT_SOURCE_DIR}/native/core/include")
target_sources(native-soa INTERFACE "${NATIVE_SOA_GENERATED}/native_soa_types.h")
target_link_libraries(native-soa INTERFACE sandbox::warnings_as_errors)

add_executable(native-soa-tests tests.cpp)
target_link_libraries(native-soa-tests PRIVATE native-soa GTest::gtest_main)
add_test(NAME native-soa-tests COMMAND native-soa-tests)
set_tests_properties(native-soa-tests PROPERTIES LABELS "native-soa")

add_executable(native-soa-benchmarks benchmarks.cpp bulk_benchmarks.cpp)
target_link_libraries(native-soa-benchmarks PRIVATE native-soa benchmark::benchmark_main)
add_executable(native-soa-reserve-matrix reserve_matrix.cpp)
target_link_libraries(native-soa-reserve-matrix PRIVATE native-soa benchmark::benchmark_main)
target_compile_definitions(native-soa-reserve-matrix PRIVATE NATIVE_SOA_MIMALLOC=0)
add_executable(native-soa-reserve-matrix-mimalloc reserve_matrix.cpp)
target_compile_definitions(native-soa-reserve-matrix-mimalloc PRIVATE NATIVE_SOA_MIMALLOC=1)
target_link_libraries(native-soa-reserve-matrix-mimalloc PRIVATE native-soa benchmark::benchmark_main
  $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>)
add_executable(native-soa-tests-mimalloc tests.cpp)
target_compile_definitions(native-soa-tests-mimalloc PRIVATE NATIVE_SOA_MIMALLOC=1)
target_link_libraries(native-soa-tests-mimalloc PRIVATE native-soa GTest::gtest_main
  $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>)
add_test(NAME native-soa-tests-mimalloc COMMAND native-soa-tests-mimalloc)
set_tests_properties(native-soa-tests-mimalloc PROPERTIES LABELS "native-soa")

foreach(matrix_target IN ITEMS native-soa-reserve-matrix native-soa-reserve-matrix-mimalloc native-soa-tests-mimalloc)
  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(${matrix_target} PRIVATE /fp:precise /arch:AVX2 /Zi)
    target_link_options(${matrix_target} PRIVATE /DEBUG)
  else()
    target_compile_options(${matrix_target} PRIVATE -mavx2 -ffp-contract=off -g)
    if(WIN32)
      target_compile_options(${matrix_target} PRIVATE -gcodeview)
      target_link_options(${matrix_target} PRIVATE -Xlinker /DEBUG)
    endif()
  endif()
endforeach()
if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(native-soa-benchmarks PRIVATE /fp:precise /arch:AVX2 /Zi)
  target_link_options(native-soa-benchmarks PRIVATE /DEBUG)
else()
  target_compile_options(native-soa-benchmarks PRIVATE -mavx2 -ffp-contract=off)
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

add_custom_target(native-soa-reserve-matrix-report
  COMMAND "${UV_EXECUTABLE}" run "${PROJECT_SOURCE_DIR}/Scripts/run-native-soa-reserve-matrix.py"
    --standard "$<TARGET_FILE:native-soa-reserve-matrix>"
    --mimalloc "$<TARGET_FILE:native-soa-reserve-matrix-mimalloc>"
    --output-dir "${CMAKE_BINARY_DIR}/native-soa-reserve-matrix"
  DEPENDS native-soa-reserve-matrix native-soa-reserve-matrix-mimalloc USES_TERMINAL VERBATIM)
add_test(NAME native-soa-reserve-matrix-smoke
  COMMAND "${UV_EXECUTABLE}" run "${PROJECT_SOURCE_DIR}/Scripts/run-native-soa-reserve-matrix.py"
    --standard "$<TARGET_FILE:native-soa-reserve-matrix>"
    --mimalloc "$<TARGET_FILE:native-soa-reserve-matrix-mimalloc>"
    --rows 4096 --owners 1 4 --dry-run
    --output-dir "${CMAKE_BINARY_DIR}/native-soa-reserve-matrix-smoke")
set_tests_properties(native-soa-reserve-matrix-smoke PROPERTIES LABELS "native-soa")

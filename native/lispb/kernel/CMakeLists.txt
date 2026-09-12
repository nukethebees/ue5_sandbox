add_library(kernel-codegen-lib
  src/compiler.cpp
  src/lowering.cpp
  src/manifest.cpp
  src/parser.cpp
  src/reference_evaluator.cpp
  src/renderer.cpp
  src/avx2_lab_renderer.cpp
  src/standard_renderer.cpp
  src/test_renderer.cpp
  src/unreal_renderer.cpp
)
target_compile_features(kernel-codegen-lib PUBLIC cxx_std_23)
target_include_directories(kernel-codegen-lib PUBLIC include)
target_link_libraries(kernel-codegen-lib PRIVATE
  lispb-output
  lispb-syntax
  sandbox::codegen_warnings
)

add_executable(kernel-codegen-tests
  tests/compiler_io_tests.cpp
  tests/compiler_tests.cpp
)
target_include_directories(kernel-codegen-tests PRIVATE src)
target_link_libraries(kernel-codegen-tests PRIVATE
  codegen-lib
  kernel-codegen-lib
  lispb-syntax
  GTest::gtest_main
  sandbox::codegen_warnings
)

gtest_discover_tests(kernel-codegen-tests PROPERTIES LABELS "unit;all;codegen")

set(KERNEL_COMPILE_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile")
set(KERNEL_COMPILE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compile-generated")
set(KERNEL_COMPILE_FILES
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.h"
  "${KERNEL_COMPILE_OUTPUT}/ArrayKernels.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_COMPILE_FILES}
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-compile-fixture
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_COMPILE_FIXTURE}/array_math.lispb"
  VERBATIM
)
add_executable(kernel-generated-compile-tests
  tests/compile/main.cpp
  ${KERNEL_COMPILE_FILES}
)
target_compile_features(kernel-generated-compile-tests PRIVATE cxx_std_23)
target_compile_definitions(kernel-generated-compile-tests PRIVATE CODEGEN_CHECK_EXIT)
target_include_directories(kernel-generated-compile-tests PRIVATE
  "${KERNEL_COMPILE_OUTPUT}"
  "${CMAKE_SOURCE_DIR}/native/lispb/tests/compile_fixture/stubs"
)
target_link_libraries(kernel-generated-compile-tests PRIVATE sandbox::codegen_warnings)
add_dependencies(kernel-codegen-tests kernel-generated-compile-tests)
add_test(NAME KernelGeneratedCompileSmoke COMMAND kernel-generated-compile-tests)
set_tests_properties(KernelGeneratedCompileSmoke PROPERTIES LABELS "unit;all;codegen")
foreach(INVARIANT_CASE IN ITEMS unequal-lengths overlapping-output overlapping-in-place)
  add_test(
    NAME "KernelGeneratedInvariant.${INVARIANT_CASE}"
    COMMAND kernel-generated-compile-tests "${INVARIANT_CASE}"
  )
  set_tests_properties(
    "KernelGeneratedInvariant.${INVARIANT_CASE}"
    PROPERTIES
      LABELS "unit;all;codegen"
      WILL_FAIL TRUE
  )
endforeach()

set(KERNEL_PRODUCTION_ROOT
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Source/SandboxCore"
)
set(KERNEL_PRODUCTION_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/array_math.lispb")
set(KERNEL_CANDIDATE_DOCUMENT "${KERNEL_PRODUCTION_ROOT}/Kernels/candidate_math.lispb")

set(KERNEL_STANDARD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/standard-generated")
set(KERNEL_STANDARD_FILES
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/array_math_kernels_tests.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.h"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels.cpp"
  "${KERNEL_STANDARD_OUTPUT}/standard/candidate_math_kernels_tests.cpp"
)
add_custom_command(
  OUTPUT ${KERNEL_STANDARD_FILES}
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-standard
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  COMMENT "Generating standard C++ kernel tests"
  VERBATIM
)
add_executable(kernel-generated-standard-tests
  tests/standard_anchor_tests.cpp
  ${KERNEL_STANDARD_FILES}
)
target_compile_features(kernel-generated-standard-tests PRIVATE cxx_std_23)
target_include_directories(kernel-generated-standard-tests PRIVATE "${KERNEL_STANDARD_OUTPUT}")
target_link_libraries(kernel-generated-standard-tests PRIVATE
  GTest::gtest_main
  sandbox::codegen_warnings
)
gtest_discover_tests(kernel-generated-standard-tests PROPERTIES LABELS "unit;all;codegen")
add_dependencies(kernel-codegen-tests kernel-generated-standard-tests)

add_custom_target(generate-kernel-code
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-unreal
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-kernel-code
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-unreal
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_PRODUCTION_DOCUMENT}"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed SandboxCore kernel code"
  USES_TERMINAL
  VERBATIM
)

set(KERNEL_AVX2_LAB_ROOT
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private"
)
set(KERNEL_AVX2_LAB_FILES
  "${KERNEL_AVX2_LAB_ROOT}/generated/add_scaled_avx2_lab.h"
  "${KERNEL_AVX2_LAB_ROOT}/generated/add_scaled_avx2_lab.cpp"
)
add_custom_target(generate-kernel-avx2-lab
  COMMAND lispb generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-avx2-lab
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating the private AVX2 kernel experiment"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(check-generated-kernel-avx2-lab
  COMMAND lispb check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target kernel-avx2-lab
  DEPENDS
    lispb
    "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    "${KERNEL_CANDIDATE_DOCUMENT}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking the private AVX2 kernel experiment"
  USES_TERMINAL
  VERBATIM
)

add_executable(kernel-generated-avx2-lab-tests
  tests/avx2_lab_tests.cpp
  ${KERNEL_AVX2_LAB_FILES}
)
target_compile_features(kernel-generated-avx2-lab-tests PRIVATE cxx_std_23)
target_include_directories(kernel-generated-avx2-lab-tests PRIVATE
  "${KERNEL_AVX2_LAB_ROOT}"
  "${CMAKE_SOURCE_DIR}/native/lispb/tests/compile_fixture/stubs"
)
target_link_libraries(kernel-generated-avx2-lab-tests PRIVATE
  GTest::gtest_main
  sandbox::codegen_warnings
)
if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(kernel-generated-avx2-lab-tests PRIVATE /arch:AVX2 /fp:precise)
else()
  target_compile_options(kernel-generated-avx2-lab-tests PRIVATE
    -mavx2
    -ffp-contract=off
  )
endif()
add_dependencies(kernel-generated-avx2-lab-tests check-generated-kernel-avx2-lab)
add_dependencies(kernel-codegen-tests kernel-generated-avx2-lab-tests)
gtest_discover_tests(kernel-generated-avx2-lab-tests PROPERTIES LABELS "unit;all;codegen")

if(SANDBOX_KERNEL_BENCHMARKS)
  set(KERNEL_NATIVE_SIMD_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native-simd-generated")
  set(KERNEL_NATIVE_SIMD_HEADER
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/add_scaled_x86_simd_lab.h"
  )
  set(KERNEL_NATIVE_SIMD_AVX2_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/add_scaled_x86_simd_lab_avx2.cpp"
  )
  set(KERNEL_NATIVE_SIMD_AVX512_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/add_scaled_x86_simd_lab_avx512.cpp"
  )
  set(KERNEL_NATIVE_DOT_HEADER
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_x86_simd_lab.h"
  )
  set(KERNEL_NATIVE_DOT_AVX2_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_x86_simd_lab_avx2.cpp"
  )
  set(KERNEL_NATIVE_DOT_AVX512_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_x86_simd_lab_avx512.cpp"
  )
  set(KERNEL_NATIVE_DOT_RELAXED_AVX2_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_x86_simd_lab_relaxed_avx2.cpp"
  )
  set(KERNEL_NATIVE_DOT_RELAXED_AVX512_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_x86_simd_lab_relaxed_avx512.cpp"
  )
  set(KERNEL_NATIVE_DOT3_HEADER
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_3d_x86_simd_lab.h"
  )
  set(KERNEL_NATIVE_DOT3_AVX2_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_3d_x86_simd_lab_avx2.cpp"
  )
  set(KERNEL_NATIVE_DOT3_AVX512_SOURCE
    "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated/dot_product_3d_x86_simd_lab_avx512.cpp"
  )
  set(KERNEL_NATIVE_SIMD_FILES
    "${KERNEL_NATIVE_SIMD_HEADER}"
    "${KERNEL_NATIVE_SIMD_AVX2_SOURCE}"
    "${KERNEL_NATIVE_SIMD_AVX512_SOURCE}"
    "${KERNEL_NATIVE_DOT_HEADER}"
    "${KERNEL_NATIVE_DOT_AVX2_SOURCE}"
    "${KERNEL_NATIVE_DOT_AVX512_SOURCE}"
    "${KERNEL_NATIVE_DOT_RELAXED_AVX2_SOURCE}"
    "${KERNEL_NATIVE_DOT_RELAXED_AVX512_SOURCE}"
    "${KERNEL_NATIVE_DOT3_HEADER}"
    "${KERNEL_NATIVE_DOT3_AVX2_SOURCE}"
    "${KERNEL_NATIVE_DOT3_AVX512_SOURCE}"
  )
  add_custom_command(
    OUTPUT ${KERNEL_NATIVE_SIMD_FILES}
    COMMAND lispb generate
      --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
      --build-root "${CMAKE_BINARY_DIR}"
      --target kernel-native-simd
    DEPENDS
      lispb
      "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
      "${KERNEL_CANDIDATE_DOCUMENT}"
    COMMENT "Generating the native x86 SIMD kernel experiment"
    VERBATIM
  )
  set(KERNEL_NATIVE_SIMD_CONVENIENCE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/out/benchmarks/kernel/generated"
  )
  add_custom_target(kernel-native-generated-sources
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${KERNEL_NATIVE_SIMD_CONVENIENCE_DIRECTORY}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
      "${KERNEL_NATIVE_SIMD_OUTPUT}/native/generated"
      "${KERNEL_NATIVE_SIMD_CONVENIENCE_DIRECTORY}"
    DEPENDS ${KERNEL_NATIVE_SIMD_FILES}
    COMMENT "Updating the stable native SIMD generated-source mirror"
    VERBATIM
  )

  add_library(kernel-native-simd-avx2 OBJECT
    "${KERNEL_NATIVE_SIMD_AVX2_SOURCE}"
    "${KERNEL_NATIVE_DOT_AVX2_SOURCE}"
    "${KERNEL_NATIVE_DOT3_AVX2_SOURCE}"
  )
  add_library(kernel-native-simd-avx512 OBJECT
    "${KERNEL_NATIVE_SIMD_AVX512_SOURCE}"
    "${KERNEL_NATIVE_DOT_AVX512_SOURCE}"
    "${KERNEL_NATIVE_DOT3_AVX512_SOURCE}"
  )
  add_library(kernel-native-dot-relaxed-avx2 OBJECT
    "${KERNEL_NATIVE_DOT_RELAXED_AVX2_SOURCE}"
  )
  add_library(kernel-native-dot-relaxed-avx512 OBJECT
    "${KERNEL_NATIVE_DOT_RELAXED_AVX512_SOURCE}"
  )
  foreach(TARGET IN ITEMS
      kernel-native-simd-avx2
      kernel-native-simd-avx512
      kernel-native-dot-relaxed-avx2
      kernel-native-dot-relaxed-avx512)
    target_compile_features(${TARGET} PRIVATE cxx_std_23)
    target_include_directories(${TARGET} PRIVATE "${KERNEL_NATIVE_SIMD_OUTPUT}")
    target_link_libraries(${TARGET} PRIVATE sandbox::codegen_warnings)
  endforeach()
  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(kernel-native-simd-avx2 PRIVATE /arch:AVX2 /fp:strict)
    target_compile_options(kernel-native-simd-avx512 PRIVATE /arch:AVX512 /fp:strict)
    target_compile_options(kernel-native-dot-relaxed-avx2 PRIVATE
      /arch:AVX2 /fp:fast
    )
    target_compile_options(kernel-native-dot-relaxed-avx512 PRIVATE
      /arch:AVX512 /fp:fast
    )
  else()
    target_compile_options(kernel-native-simd-avx2 PRIVATE
      -mavx2
      -ffp-contract=off
    )
    target_compile_options(kernel-native-simd-avx512 PRIVATE
      -mavx2
      -mavx512f
      -mavx512cd
      -mavx512bw
      -mavx512dq
      -mavx512vl
      -ffp-contract=off
    )
    target_compile_options(kernel-native-dot-relaxed-avx2 PRIVATE
      -mavx2
      -fno-signed-zeros
      -fno-trapping-math
      -fassociative-math
      -ffp-contract=off
    )
    target_compile_options(kernel-native-dot-relaxed-avx512 PRIVATE
      -mavx2
      -mavx512f
      -mavx512cd
      -mavx512bw
      -mavx512dq
      -mavx512vl
      -fno-signed-zeros
      -fno-trapping-math
      -fassociative-math
      -ffp-contract=off
    )
  endif()

  set(KERNEL_NATIVE_SIMD_OBJECTS
    $<TARGET_OBJECTS:kernel-native-simd-avx2>
    $<TARGET_OBJECTS:kernel-native-simd-avx512>
    $<TARGET_OBJECTS:kernel-native-dot-relaxed-avx2>
    $<TARGET_OBJECTS:kernel-native-dot-relaxed-avx512>
  )
  add_executable(kernel-native-simd-tests
    tests/x86_simd_lab_tests.cpp
    ${KERNEL_NATIVE_SIMD_OBJECTS}
  )
  add_dependencies(kernel-native-simd-tests kernel-native-generated-sources)
  target_compile_features(kernel-native-simd-tests PRIVATE cxx_std_23)
  target_include_directories(kernel-native-simd-tests PRIVATE "${KERNEL_NATIVE_SIMD_OUTPUT}")
  target_link_libraries(kernel-native-simd-tests PRIVATE
    CpuFeatures::cpu_features
    GTest::gtest_main
    sandbox::codegen_warnings
  )
  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(kernel-native-simd-tests PRIVATE /arch:AVX2 /fp:strict)
  else()
    target_compile_options(kernel-native-simd-tests PRIVATE
      -mavx2
      -ffp-contract=off
    )
  endif()
  gtest_discover_tests(kernel-native-simd-tests
    PROPERTIES LABELS "unit;native-kernel-benchmark"
  )

  add_executable(kernel-native-benchmarks
    benchmarks/add_scaled_benchmarks.cpp
    benchmarks/dot_product_benchmarks.cpp
    benchmarks/dot_product_3d_benchmarks.cpp
    ${KERNEL_NATIVE_SIMD_OBJECTS}
  )
  add_dependencies(kernel-native-benchmarks kernel-native-generated-sources)
  target_compile_features(kernel-native-benchmarks PRIVATE cxx_std_23)
  target_include_directories(kernel-native-benchmarks PRIVATE "${KERNEL_NATIVE_SIMD_OUTPUT}")
  target_link_libraries(kernel-native-benchmarks PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    CpuFeatures::cpu_features
    sandbox::codegen_warnings
  )
  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(kernel-native-benchmarks PRIVATE /arch:AVX2 /fp:strict)
  else()
    target_compile_options(kernel-native-benchmarks PRIVATE
      -mavx2
      -ffp-contract=off
    )
  endif()
  add_test(
    NAME KernelNativeBenchmarkDryRun
    COMMAND kernel-native-benchmarks --benchmark_dry_run
  )
  set_tests_properties(KernelNativeBenchmarkDryRun
    PROPERTIES LABELS "native-kernel-benchmark"
  )

  if(SANDBOX_KERNEL_BENCHMARK_PLOTS)
    set(KERNEL_BENCHMARK_PLOT_SCRIPT
      "${PROJECT_SOURCE_DIR}/Scripts/plot-kernel-benchmarks.py"
    )
    set(KERNEL_BENCHMARK_PLOT_TEST
      "${PROJECT_SOURCE_DIR}/Scripts/test_plot_kernel_benchmarks.py"
    )
    set(KERNEL_BENCHMARK_SCRIPT_LOCK
      "${PROJECT_SOURCE_DIR}/Scripts/plot-kernel-benchmarks.py.lock"
    )
    set(KERNEL_BENCHMARK_RESULTS_DIRECTORY
      "${PROJECT_SOURCE_DIR}/out/benchmarks/kernel"
    )
    set(KERNEL_BENCHMARK_RESULTS
      "${KERNEL_BENCHMARK_RESULTS_DIRECTORY}/results.json"
    )
    set(KERNEL_BENCHMARK_PLOTS_DIRECTORY
      "${KERNEL_BENCHMARK_RESULTS_DIRECTORY}/plots"
    )
    set(KERNEL_BENCHMARK_UV_CACHE
      "${PROJECT_SOURCE_DIR}/out/uv-cache"
    )
    set(KERNEL_BENCHMARK_MATPLOTLIB_CACHE
      "${PROJECT_SOURCE_DIR}/out/matplotlib-cache"
    )
    set(KERNEL_BENCHMARK_TEST_TEMP
      "${PROJECT_SOURCE_DIR}/out/test-temp"
    )
    file(MAKE_DIRECTORY
      "${KERNEL_BENCHMARK_UV_CACHE}"
      "${KERNEL_BENCHMARK_MATPLOTLIB_CACHE}"
      "${KERNEL_BENCHMARK_TEST_TEMP}"
    )

    function(add_kernel_benchmark_report target_name)
      cmake_parse_arguments(REPORT "" "FILTER;COMMENT" "" ${ARGN})
      set(filter_argument)
      if(REPORT_FILTER)
        set(filter_argument "--benchmark_filter=${REPORT_FILTER}")
      endif()

      add_custom_target(${target_name}
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
          "${KERNEL_BENCHMARK_PLOTS_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f
          "${KERNEL_BENCHMARK_RESULTS}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
          "${KERNEL_BENCHMARK_RESULTS_DIRECTORY}"
          "${KERNEL_BENCHMARK_PLOTS_DIRECTORY}"
        COMMAND kernel-native-benchmarks
          --benchmark_repetitions=7
          --benchmark_min_time=0.05s
          --benchmark_enable_random_interleaving=true
          --benchmark_display_aggregates_only=true
          ${filter_argument}
          "--benchmark_out=${KERNEL_BENCHMARK_RESULTS}"
          --benchmark_out_format=json
        COMMAND "${CMAKE_COMMAND}" -E env
          "UV_CACHE_DIR=${KERNEL_BENCHMARK_UV_CACHE}"
          "MPLCONFIGDIR=${KERNEL_BENCHMARK_MATPLOTLIB_CACHE}"
          "${UV_EXECUTABLE}" run --locked
            "${KERNEL_BENCHMARK_PLOT_SCRIPT}"
            "${KERNEL_BENCHMARK_RESULTS}"
            "--output=${KERNEL_BENCHMARK_PLOTS_DIRECTORY}"
            --format=png
        DEPENDS
          kernel-native-benchmarks
          "${KERNEL_BENCHMARK_PLOT_SCRIPT}"
          "${KERNEL_BENCHMARK_SCRIPT_LOCK}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "${REPORT_COMMENT}"
        USES_TERMINAL
        VERBATIM
      )
    endfunction()

    add_kernel_benchmark_report(kernel-benchmark-report
      FILTER "^(add_scaled/elementwise|dot_product/relaxed)/(flat|chunked16)/[^/]+/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
      COMMENT "Benchmarking representative generated kernel workloads and plotting the results"
    )
    add_kernel_benchmark_report(kernel-vector-layout-benchmark-report
      FILTER "^dot_product_3d/elementwise/(aos/(scalar|autovec-avx2|avx2|autovec-avx512|avx512)|(soa-flat|soa-chunked16)/(autovec-avx2|avx2|autovec-avx512|avx512))/ordinary/aligned/(4096|16384|65536|100000)/real_time$"
      COMMENT "Benchmarking generated 3D dot products across AOS and SOA layouts"
    )
    add_kernel_benchmark_report(kernel-benchmark-report-full
      COMMENT "Benchmarking the comprehensive generated kernel matrix and plotting the results"
    )

    add_test(
      NAME KernelBenchmarkPlotTests
      COMMAND "${Python3_EXECUTABLE}"
        "${KERNEL_BENCHMARK_PLOT_TEST}"
        "--uv=${UV_EXECUTABLE}"
        "--temporary-root=${KERNEL_BENCHMARK_TEST_TEMP}"
    )
    set_tests_properties(KernelBenchmarkPlotTests PROPERTIES
      LABELS "kernel-benchmark-plot"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      ENVIRONMENT
        "UV_CACHE_DIR=${KERNEL_BENCHMARK_UV_CACHE};MPLCONFIGDIR=${KERNEL_BENCHMARK_MATPLOTLIB_CACHE};TMP=${KERNEL_BENCHMARK_TEST_TEMP};TEMP=${KERNEL_BENCHMARK_TEST_TEMP}"
    )
  endif()
endif()

add_dependencies(generate-code generate-kernel-code)
add_dependencies(generate-code generate-kernel-avx2-lab)
add_dependencies(check-generated-code check-generated-kernel-code)
add_dependencies(check-generated-code check-generated-kernel-avx2-lab)

if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "native-core requires an MSVC-compatible ABI on Windows.")
endif()

add_library(native-core STATIC)
add_library(native-core-headers INTERFACE)
target_include_directories(native-core-headers
  INTERFACE
    include
)
target_sources(native-core
  PRIVATE
    include/sandbox/core/container_ops.h
    include/sandbox/core/monotonic_clock.h
    include/sandbox/core/speed_response.h
    include/sandbox/core/diagnostics.h
    include/sandbox/core/enum_traits.h
    include/sandbox/core/packed_value.h
    include/sandbox/core/parallel_for.h
    include/sandbox/core/countdown_timers.h
    include/sandbox/core/math_types.h
    include/sandbox/core/native_soa/storage.h
    include/sandbox/core/native_soa/vector_storage_ops.h
    include/sandbox/core/periodic_tick_countdown.h
    include/sandbox/core/space_dust_math.h
    include/sandbox/core/tick_countdown.h
    include/sandbox/core/vector_normalization.h
    src/generated/array_math_kernels.cpp
    src/generated/vector_lerp_kernels.cpp
    src/files.cpp
    src/rotator3d.cpp
    src/array_math.cpp
    src/array_utils.cpp
    src/countdown.cpp
    src/trigonometry.cpp
    src/frame_memory_resource.cpp
    src/projectile_intercept.cpp
    src/rotation.cpp
    src/test_timeline.cpp
)
target_include_directories(native-core
  PUBLIC
    include
)

# Keep file-only static-library consumers independent of the runtime allocation/profiling code.
set_source_files_properties(src/files.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

# Separate translation units keep AVX2 out of the scalar reference and prevent
# unity builds or contraction from changing the packing arithmetic.
add_library(native-core-ismc-scalar OBJECT)
target_sources(native-core-ismc-scalar
  PRIVATE
    src/sandbox_ismc_packing.cpp
    src/sandbox_ismc_packing_dispatch.cpp
)
set_target_properties(native-core-ismc-scalar PROPERTIES UNITY_BUILD OFF)
target_include_directories(native-core-ismc-scalar
  PRIVATE
    include
)
target_link_libraries(native-core-ismc-scalar
  PRIVATE
    hwy sandbox::native_defaults sandbox::handmade_math ioj::fp_precise
)
ioj_apply_native_abi(native-core-ismc-scalar)
ioj_enable_warnings(native-core-ismc-scalar)
add_library(native-core-ismc-highway-avx2 OBJECT)
target_sources(native-core-ismc-highway-avx2
  PRIVATE
    src/sandbox_ismc_packing_highway.cpp
)
ioj_configure_highway_target(native-core-ismc-highway-avx2 avx2)
target_include_directories(native-core-ismc-highway-avx2
  PRIVATE
    include
)
target_link_libraries(native-core-ismc-highway-avx2
  PRIVATE
    sandbox::handmade_math
)
target_link_libraries(native-core
  PUBLIC
    sandbox::native_defaults
    TBB::tbb
    sandbox::handmade_math
  PRIVATE
    native-core-ismc-scalar
    native-core-ismc-highway-avx2
    ioj::core_pch
    "$<$<BOOL:${IOJ_WITH_TRACY}>:sandbox::profiling>"
    "$<$<BOOL:${IOJ_WITH_UNREAL}>:ioj::unreal_avx2>"
)
ioj_enable_warnings(native-core)
if(IOJ_WITH_TRACY)
  target_compile_definitions(native-core
    PRIVATE
      IOJ_WITH_TRACY
  )
endif()

set_target_properties(native-core PROPERTIES
  OUTPUT_NAME SandboxNativeCore
)
if(IOJ_WITH_UNREAL)
  set_target_properties(native-core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${IOJ_NATIVE_OUTPUT_ROOT}/Core/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_executable(native-core-tests)
ioj_stage_tbb_runtime(native-core-tests)

target_sources(native-core-tests
  PRIVATE
    tests/generated/array_math_kernels_tests.cpp
    tests/array_math_tests.cpp
    tests/vector_lerp_tests.cpp
    tests/array_utils_tests.cpp
    tests/container_tests.cpp
    tests/fixed_array_tests.cpp
    tests/frame_array_tests.cpp
    tests/frame_memory_resource_tests.cpp
    tests/countdown_tests.cpp
    tests/enum_array_tests.cpp
    tests/interpolation_tests.cpp
    tests/invoke_tests.cpp
    tests/lock_free_mpsc_queue_tests.cpp
    tests/loop_bounds_tests.cpp
    tests/monitored_lock_free_mpsc_queue_tests.cpp
    tests/native_soa_vector_storage_ops_tests.cpp
    tests/projectile_intercept_tests.cpp
    tests/quantized_decimal_tests.cpp
    tests/rotation_tests.cpp
    tests/sandbox_ismc_transform_tests.cpp
    tests/sandbox_ismc_packing_tests.cpp
    tests/space_dust_math_tests.cpp
    tests/static_string_view_tests.cpp
    tests/time_series_data_tests.cpp
    tests/timing_tests.cpp
    tests/test_timeline_tests.cpp
    tests/vector_math_tests.cpp
    tests/vector3d_tests.cpp
)
ioj_enable_warnings(native-core-tests)

add_test(NAME native-core-tests COMMAND native-core-tests)
set_tests_properties(native-core-tests PROPERTIES LABELS "unit;all;native;native-core")

add_library(native-core-ismc-avx2-experiment OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-ismc-avx2-experiment
  PRIVATE
    experiments/sandbox_ismc_packing_avx2.cpp
)
set_target_properties(native-core-ismc-avx2-experiment PROPERTIES UNITY_BUILD OFF)
target_include_directories(native-core-ismc-avx2-experiment
  PUBLIC
    experiments
)
target_link_libraries(native-core-ismc-avx2-experiment
  PUBLIC
    native-core
  PRIVATE
    sandbox::native_defaults ioj::arch_avx2 ioj::fp_precise
)
ioj_apply_native_abi(native-core-ismc-avx2-experiment)
ioj_enable_warnings(native-core-ismc-avx2-experiment)

add_library(native-core-ismc-avx512-experiment OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-ismc-avx512-experiment
  PRIVATE
    experiments/sandbox_ismc_packing_avx512.cpp
)
set_target_properties(native-core-ismc-avx512-experiment PROPERTIES UNITY_BUILD OFF)
target_include_directories(native-core-ismc-avx512-experiment
  PUBLIC
    experiments
)
target_link_libraries(native-core-ismc-avx512-experiment
  PUBLIC
    native-core CpuFeatures::cpu_features
  PRIVATE
    sandbox::native_defaults ioj::arch_avx512 ioj::fp_precise
)
ioj_apply_native_abi(native-core-ismc-avx512-experiment)
ioj_enable_warnings(native-core-ismc-avx512-experiment)

add_library(native-core-ismc-highway-avx512 OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-ismc-highway-avx512
  PRIVATE
    src/sandbox_ismc_packing_highway.cpp
)
ioj_configure_highway_target(native-core-ismc-highway-avx512 avx512)
target_include_directories(native-core-ismc-highway-avx512
  PUBLIC
    experiments
)
target_link_libraries(native-core-ismc-highway-avx512
  PUBLIC
    native-core
)
target_link_libraries(native-core-tests
  PRIVATE
    native-core
    sandbox::native_defaults
    ioj::test_pch
    GTest::gtest_main
    CpuFeatures::cpu_features
    native-core-ismc-avx2-experiment
    native-core-ismc-avx512-experiment
    native-core-ismc-highway-avx512
    hwy
)

add_subdirectory(benchmarks)

add_library(native-core-static-string-view-reject-automatic OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-static-string-view-reject-automatic
  PRIVATE
    tests/static_string_view_reject_automatic_storage.cpp
)
target_link_libraries(native-core-static-string-view-reject-automatic
  PRIVATE
    native-core
    sandbox::native_defaults
)
ioj_enable_warnings(native-core-static-string-view-reject-automatic)
add_test(NAME StaticStringViewRejectsAutomaticStorage
  COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
    -DREJECT_TARGET=native-core-static-string-view-reject-automatic
    "-DEXPECTED_DIAGNOSTIC=(call to consteval function 'ml::StaticStringView::StaticStringView[^\n]*not a constant expression|error C7595:[^\n]*StaticStringView[^\n]*not a constant expression)"
    -P "${PROJECT_SOURCE_DIR}/cmake/expect_compile_failure.cmake"
)
set_tests_properties(StaticStringViewRejectsAutomaticStorage PROPERTIES
  LABELS "all;native;native-core;compile-contract;subprocess"
  RESOURCE_LOCK codegen-negative-build
)

ioj_apply_native_abi(native-core)
ioj_apply_native_abi(native-core-tests)

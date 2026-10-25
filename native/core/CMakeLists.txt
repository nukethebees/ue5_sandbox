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
    include/ioj/vectors3f.h
    include/ioj/vector_operations.h
    include/ioj/column_math.h
    include/ioj/rotator_types.h
    include/ioj/rotators3f.h
    include/ioj/rotator_math.h
    src/rotator_math.cpp
    include/ioj/math_types.h
    include/ioj/transform3d.h
    src/transform3d.cpp
    include/ioj/container_ops.h
    include/ioj/monotonic_clock.h
    include/ioj/speed_response.h
    include/ioj/diagnostics.h
    include/ioj/require.h
    include/ioj/constrained_numbers.h
    include/ioj/constrained_value.h
    include/ioj/structural_diff.h
    include/ioj/enum_formatter.h
    include/ioj/enum_strings.h
    include/ioj/try_numeric_cast.h
    include/ioj/constrained_vectors.h
    include/ioj/constrained_strings.h
    include/ioj/enum_traits.h
    include/ioj/field_mask.h
    include/ioj/packed_value.h
    include/ioj/parallel_for.h
    include/ioj/countdown_timers.h
    include/ioj/vector.h
    include/ioj/component_sources.h
    include/ioj/vector_value_math.h
    include/ioj/quaternion.h
    include/ioj/math_types.h
    include/ioj/native_soa/storage.h
    include/ioj/periodic_tick_countdown.h
    include/ioj/tick_countdown.h
    include/ioj/vector_normalization.h
    src/generated/array_math_kernels.cpp
    src/generated/vector_lerp_kernels.cpp
    src/files.cpp
    src/require.cpp
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

# Keep runtime allocation/profiling separate from other core utilities.
set_source_files_properties(
  src/files.cpp
  src/frame_memory_resource.cpp
  src/require.cpp
  PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
)

target_link_libraries(native-core
  PUBLIC
    sandbox::native_defaults
    TBB::tbb

  PRIVATE
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
add_library(native-test-support INTERFACE)
target_include_directories(native-test-support
  INTERFACE
    tests/include
)
target_link_libraries(native-test-support
  INTERFACE
    native-core-headers
    GTest::gtest
)
ioj_stage_tbb_runtime(native-core-tests)

target_sources(native-core-tests
  PRIVATE
    tests/generated/array_math_kernels_tests.cpp
    tests/array_math_tests.cpp
    tests/vector_lerp_tests.cpp
    tests/array_utils_tests.cpp
    tests/container_tests.cpp
    tests/constrained_value_tests.cpp
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
    tests/projectile_intercept_tests.cpp
    tests/quantized_decimal_tests.cpp
    tests/rotation_tests.cpp
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

target_link_libraries(native-core-tests
  PRIVATE
    native-core
    native-test-support
    sandbox::native_defaults
    ioj::test_pch
    GTest::gtest_main
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
    "-DEXPECTED_DIAGNOSTIC=(call to consteval function 'ioj::StaticStringView::StaticStringView[^\n]*not a constant expression|error C7595:[^\n]*StaticStringView[^\n]*not a constant expression)"
    -P "${PROJECT_SOURCE_DIR}/cmake/expect_compile_failure.cmake"
)
set_tests_properties(StaticStringViewRejectsAutomaticStorage PROPERTIES
  LABELS "all;native;native-core;compile-contract;subprocess"
  RESOURCE_LOCK codegen-negative-build
)

ioj_apply_native_abi(native-core)
ioj_apply_native_abi(native-core-tests)

add_library(structural-diff-reject-unsupported OBJECT EXCLUDE_FROM_ALL)
target_sources(structural-diff-reject-unsupported
  PRIVATE
    tests/structural_diff_reject_unsupported.cpp
)
target_link_libraries(structural-diff-reject-unsupported
  PRIVATE
    native-core-headers
    sandbox::native_defaults
)
add_test(NAME StructuralDiffRejectsUnsupportedType
  COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
    -DREJECT_TARGET=structural-diff-reject-unsupported
    "-DEXPECTED_DIAGNOSTIC=Structural diff requires a generated visitor"
    -P "${PROJECT_SOURCE_DIR}/cmake/expect_compile_failure.cmake"
)
set_tests_properties(StructuralDiffRejectsUnsupportedType PROPERTIES
  LABELS "all;native;native-core;codegen;compile-contract;subprocess"
  RESOURCE_LOCK codegen-negative-build
)

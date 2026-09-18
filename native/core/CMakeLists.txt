if(WIN32 AND
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  message(FATAL_ERROR "native-core requires an MSVC-compatible ABI on Windows.")
endif()

add_library(native-core STATIC)
target_sources(native-core PRIVATE
  include/sandbox/core/monotonic_clock.h
  include/sandbox/core/speed_response.h
  include/sandbox/core/diagnostics.h
  include/sandbox/core/countdown_timers.h
  include/sandbox/core/math_types.h
  include/sandbox/core/periodic_tick_countdown.h
  include/sandbox/core/tick_countdown.h
  include/sandbox/core/vector_normalization.h
  src/generated/array_math_kernels.cpp
  src/generated/vector_lerp_kernels.cpp
  src/array_math.cpp
  src/array_utils.cpp
  src/countdown.cpp
  src/trigonometry.cpp
  src/frame_memory_resource.cpp
  src/graph_plot.cpp
  src/projectile_intercept.cpp
  src/rotation.cpp
  src/test_timeline.cpp
)
target_compile_features(native-core PUBLIC cxx_std_23)
target_include_directories(native-core PUBLIC include)
target_link_libraries(native-core
  PUBLIC sandbox::handmade_math
  PRIVATE sandbox::warnings_as_errors
)
if(SANDBOX_WITH_TRACY)
  target_compile_definitions(native-core PRIVATE SANDBOX_WITH_TRACY)
  target_link_libraries(native-core PRIVATE sandbox::profiling)
endif()

set_target_properties(native-core PROPERTIES
  OUTPUT_NAME SandboxNativeCore
)
if(SANDBOX_WITH_UNREAL)
  target_link_libraries(native-core PRIVATE sandbox::unreal_avx2)
  set_target_properties(native-core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/Core/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_executable(native-core-tests)

target_sources(native-core-tests PRIVATE
  tests/generated/array_math_kernels_tests.cpp
  tests/vector_lerp_tests.cpp
  tests/array_utils_tests.cpp
  tests/container_tests.cpp
  tests/countdown_tests.cpp
  tests/enum_array_tests.cpp
  tests/graph_plot_tests.cpp
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
)
target_compile_features(native-core-tests PRIVATE cxx_std_23)
target_link_libraries(native-core-tests PRIVATE native-core sandbox::warnings_as_errors)

target_link_libraries(native-core-tests PRIVATE GTest::gtest_main)

add_test(NAME native-core-tests COMMAND native-core-tests)
set_tests_properties(native-core-tests PROPERTIES LABELS "unit;all;native-core")

if(SANDBOX_KERNEL_BENCHMARKS)
  add_executable(native-core-vector-lerp-benchmarks)
  target_sources(native-core-vector-lerp-benchmarks PRIVATE benchmarks/vector_lerp_benchmarks.cpp)
  target_compile_features(native-core-vector-lerp-benchmarks PRIVATE cxx_std_23)
  target_link_libraries(native-core-vector-lerp-benchmarks PRIVATE
    native-core
    benchmark::benchmark
    benchmark::benchmark_main
    sandbox::warnings_as_errors
  )

  add_custom_target(native-core-vector-lerp-benchmark-report
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_SOURCE_DIR}/out/benchmarks/native-core"
    COMMAND ${benchmark_activity_command} native-core-vector-lerp-benchmarks
      --benchmark_repetitions=7
      --benchmark_min_time=0.05s
      --benchmark_enable_random_interleaving=true
      --benchmark_display_aggregates_only=true
      "--benchmark_out=${PROJECT_SOURCE_DIR}/out/benchmarks/native-core/vector-lerp-results.json"
      --benchmark_out_format=json
    DEPENDS native-core-vector-lerp-benchmarks
    COMMENT "Benchmarking fused native 3D lerp against three 1D lerps"
    USES_TERMINAL
    VERBATIM
  )
endif()

add_library(native-core-static-string-view-reject-automatic OBJECT EXCLUDE_FROM_ALL)
target_sources(native-core-static-string-view-reject-automatic PRIVATE
  tests/static_string_view_reject_automatic_storage.cpp
)
target_compile_features(native-core-static-string-view-reject-automatic PRIVATE cxx_std_23)
target_link_libraries(native-core-static-string-view-reject-automatic PRIVATE
  native-core
  sandbox::warnings_as_errors
)
add_test(NAME StaticStringViewRejectsAutomaticStorage
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
    --target native-core-static-string-view-reject-automatic
)
set_tests_properties(StaticStringViewRejectsAutomaticStorage PROPERTIES
  LABELS "unit;all;native-core"
  WILL_FAIL TRUE
)

set_property(TARGET native-core PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_definitions(native-core PRIVATE _ITERATOR_DEBUG_LEVEL=0)

if(WIN32)
  set_property(TARGET native-core-tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(native-core-tests PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endif()

if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(native-core PRIVATE /permissive-)
  if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
    target_compile_options(native-core PRIVATE /O2)
  endif()
else()
  if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
    target_compile_options(native-core PRIVATE -O2)
  endif()
endif()

if(UE_CONFIGURATION MATCHES "^(Shipping|Test)$")
  target_compile_definitions(native-core PRIVATE NDEBUG)
endif()

if(SANDBOX_MSVC_FRONTEND)
  target_compile_options(native-core-tests PRIVATE /permissive-)
endif()

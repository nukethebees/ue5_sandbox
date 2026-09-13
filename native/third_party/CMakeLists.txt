set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

foreach(google_test_target IN ITEMS gtest gtest_main)
  set_property(TARGET ${google_test_target} PROPERTY
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${google_test_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endforeach()

set(BUILD_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(cpu_features)
set_property(TARGET cpu_features PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

if(SANDBOX_WITH_UNREAL)
  target_link_libraries(cpu_features PRIVATE sandbox::unreal_avx2)
  set_target_properties(cpu_features PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/CpuFeatures/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

if(SANDBOX_KERNEL_BENCHMARKS OR SANDBOX_NATIVE_SOA_BENCHMARKS)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_DOXYGEN OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
  add_subdirectory(benchmark)

  foreach(google_benchmark_target IN ITEMS benchmark benchmark_main)
    set_property(TARGET ${google_benchmark_target} PROPERTY
      MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    target_compile_definitions(${google_benchmark_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
  endforeach()
endif()

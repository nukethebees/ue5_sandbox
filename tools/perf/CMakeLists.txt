option(SANDBOX_TRACY_TOOLS "Build the vendored Tracy command-line tools" OFF)

add_library(tracy-benchmark-comparison-lib STATIC)
target_sources(tracy-benchmark-comparison-lib PRIVATE
  lib/include/sandbox/perf/benchmark_comparison.hpp
  lib/src/benchmark_comparison.cpp
)
target_compile_features(tracy-benchmark-comparison-lib PUBLIC cxx_std_23)
target_include_directories(tracy-benchmark-comparison-lib PUBLIC lib/include)
target_link_libraries(tracy-benchmark-comparison-lib
  PUBLIC jobserver::client nlohmann_json::nlohmann_json
  PRIVATE CLI11::CLI11 sandbox::native_defaults ws2_32
)
target_compile_definitions(tracy-benchmark-comparison-lib PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

add_executable(tracy-benchmark-compare cli/main.cpp)
target_compile_features(tracy-benchmark-compare PRIVATE cxx_std_23)
target_link_libraries(tracy-benchmark-compare PRIVATE
  tracy-benchmark-comparison-lib
  sandbox::native_defaults
)
set_target_properties(tracy-benchmark-compare PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
install(TARGETS tracy-benchmark-compare
  RUNTIME DESTINATION bin
  COMPONENT tracy-benchmark-compare
)

add_executable(tracy-benchmark-compare-tests tests/benchmark_comparison_tests.cpp)
target_compile_features(tracy-benchmark-compare-tests PRIVATE cxx_std_23)
target_link_libraries(tracy-benchmark-compare-tests PRIVATE
  tracy-benchmark-comparison-lib
  GTest::gtest_main
  sandbox::native_defaults
)
target_compile_definitions(tracy-benchmark-compare-tests PRIVATE
  SANDBOX_PERF_TEST_HELPER_PATH="$<TARGET_FILE:tracy-benchmark-compare-test-helper>"
)

add_executable(tracy-benchmark-compare-test-helper tests/process_test_helper.cpp)
target_compile_features(tracy-benchmark-compare-test-helper PRIVATE cxx_std_23)
target_link_libraries(tracy-benchmark-compare-test-helper PRIVATE sandbox::native_defaults)
add_dependencies(tracy-benchmark-compare-tests tracy-benchmark-compare-test-helper)

add_test(NAME tracy-benchmark-compare-tests COMMAND tracy-benchmark-compare-tests)
set_tests_properties(tracy-benchmark-compare-tests PROPERTIES LABELS "unit;all;perf")

if(NOT SANDBOX_TRACY_TOOLS)
  return()
endif()

enable_language(C)

set(tracy_source_dir "${PROJECT_SOURCE_DIR}/native/third_party/tracy")
set(NO_STATISTICS OFF)
set(VENDOR_GUI OFF)
include("${tracy_source_dir}/cmake/CPM.cmake")
# The capture and CSV tools do not use JSON, and the repository already defines this target.
CPMRegisterPackage(json 3.12.0)
include("${tracy_source_dir}/cmake/vendor.cmake")
include("${tracy_source_dir}/cmake/server.cmake")

execute_process(
  COMMAND git -C "${tracy_source_dir}" rev-parse --short HEAD
  OUTPUT_VARIABLE tracy_git_ref
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE tracy_git_result
)
if(NOT tracy_git_result EQUAL 0)
  set(tracy_git_ref unknown)
endif()
configure_file(GitRef.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/GitRef.hpp" @ONLY)

add_executable(tracy-capture)
target_sources(tracy-capture PRIVATE
  "${tracy_source_dir}/capture/src/capture.cpp"
  "${tracy_source_dir}/capture/src/CaptureOutput.cpp"
)
target_include_directories(tracy-capture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(tracy-capture PRIVATE TracyServer TracyGetOpt)

add_executable(tracy-csvexport)
target_sources(tracy-csvexport PRIVATE
  "${tracy_source_dir}/csvexport/src/csvexport.cpp"
)
target_include_directories(tracy-csvexport PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(tracy-csvexport PRIVATE TracyServer TracyGetOpt)

target_compile_features(TracyServer PUBLIC cxx_std_20)

foreach(tracy_tool IN ITEMS tracy-capture tracy-csvexport)
  target_compile_features(${tracy_tool} PRIVATE cxx_std_20)
  set_property(TARGET ${tracy_tool} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${tracy_tool} PRIVATE
    _ITERATOR_DEBUG_LEVEL=0
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
  set_target_properties(${tracy_tool} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endforeach()

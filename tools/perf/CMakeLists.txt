option(SANDBOX_TRACY_TOOLS "Build the vendored Tracy command-line tools" OFF)

add_test(NAME perf-comparison-python-tests
  COMMAND "${Python3_EXECUTABLE}" -m unittest discover
    -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    -p "test_*.py"
)
set_tests_properties(perf-comparison-python-tests PROPERTIES LABELS "unit;all;perf")
set_tests_properties(perf-comparison-python-tests PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(NOT SANDBOX_TRACY_TOOLS)
  return()
endif()

enable_language(C)

set(tracy_source_dir "${PROJECT_SOURCE_DIR}/native/third_party/tracy")
set(NO_STATISTICS OFF)
set(VENDOR_GUI OFF)
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

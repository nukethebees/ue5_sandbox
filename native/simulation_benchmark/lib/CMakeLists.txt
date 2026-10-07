add_library(native-simulation-benchmark-lib STATIC)
target_sources(native-simulation-benchmark-lib PRIVATE
  include/sandbox/simulation_benchmark/benchmark_runner.hpp
  include/sandbox/simulation_benchmark/command_line.hpp
  src/benchmark_runner.cpp
  src/command_line.cpp
)
target_include_directories(native-simulation-benchmark-lib PUBLIC include)
target_link_libraries(native-simulation-benchmark-lib
  PUBLIC
    sandbox::native_defaults
    native-simulation
  PRIVATE
    native-level-authoring-s7
    CLI11::CLI11
)
ioj_enable_warnings(native-simulation-benchmark-lib)
target_compile_definitions(native-simulation-benchmark-lib PRIVATE
  IOJ_BENCHMARK_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
  IOJ_BENCHMARK_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)
if(IOJ_WITH_TRACY)
  target_compile_definitions(native-simulation-benchmark-lib PRIVATE
    IOJ_BENCHMARK_WITH_TRACY
    IOJ_WITH_TRACY
  )
  target_link_libraries(native-simulation-benchmark-lib PRIVATE Tracy::TracyClient)
endif()
ioj_apply_native_abi(native-simulation-benchmark-lib)

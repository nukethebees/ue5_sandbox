add_library(native-simulation-benchmark-lib STATIC)
target_sources(native-simulation-benchmark-lib PRIVATE
  include/sandbox/simulation_benchmark/benchmark_runner.hpp
  include/sandbox/simulation_benchmark/command_line.hpp
  src/benchmark_runner.cpp
  src/command_line.cpp
)
target_compile_features(native-simulation-benchmark-lib PUBLIC cxx_std_23)
target_include_directories(native-simulation-benchmark-lib PUBLIC include)
target_link_libraries(native-simulation-benchmark-lib
  PUBLIC native-simulation
  PRIVATE native-level-authoring CLI11::CLI11 sandbox::warnings_as_errors
)
target_compile_definitions(native-simulation-benchmark-lib PRIVATE
  SANDBOX_BENCHMARK_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
  SANDBOX_BENCHMARK_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)
if(SANDBOX_WITH_TRACY)
  target_compile_definitions(native-simulation-benchmark-lib PRIVATE SANDBOX_BENCHMARK_WITH_TRACY)
endif()
configure_sandbox_s7_abi(native-simulation-benchmark-lib)

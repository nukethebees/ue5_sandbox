add_executable(native-simulation-benchmark)
target_sources(native-simulation-benchmark PRIVATE main.cpp)
target_compile_features(native-simulation-benchmark PRIVATE cxx_std_23)
target_link_libraries(native-simulation-benchmark PRIVATE
  native-simulation-benchmark-lib
  sandbox::warnings_as_errors
)
configure_sandbox_s7_abi(native-simulation-benchmark)
set_target_properties(native-simulation-benchmark PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

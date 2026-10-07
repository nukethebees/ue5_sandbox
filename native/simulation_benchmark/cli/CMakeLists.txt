add_executable(native-simulation-benchmark)
ioj_stage_tbb_runtime(native-simulation-benchmark)
target_sources(native-simulation-benchmark PRIVATE main.cpp)
target_link_libraries(native-simulation-benchmark PRIVATE
  native-simulation-benchmark-lib
  sandbox::native_defaults
)
ioj_enable_warnings(native-simulation-benchmark)
ioj_apply_native_abi(native-simulation-benchmark)
set_target_properties(native-simulation-benchmark PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

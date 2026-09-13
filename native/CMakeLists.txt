option(SANDBOX_KERNEL_BENCHMARKS "Build native generated-kernel benchmarks" OFF)
option(SANDBOX_NATIVE_SOA_BENCHMARKS "Build standalone experimental SoA benchmarks" OFF)
option(SANDBOX_KERNEL_BENCHMARK_PLOTS "Generate native kernel benchmark plots" OFF)

add_subdirectory(third_party)
add_subdirectory(third_party/handmade_math)
add_subdirectory(sbx_mimalloc)
add_subdirectory(lispb)
add_subdirectory(memory)
add_subdirectory(s7)
add_subdirectory(core)
add_subdirectory(simulation)
add_subdirectory(image)
add_subdirectory(mesh_gen)

sandbox_target_precompile_common_headers(
  native-core
  native-simulation
  sandbox-image
)
sandbox_target_precompile_codegen_headers(
  codegen-lib
  kernel-codegen-lib
  material-gen
  sandbox-material-gen
  slate-codegen-lib
)
sandbox_target_precompile_test_headers(
  codegen-tests
  kernel-codegen-tests
  native-core-tests
  native-simulation-tests
  sandbox-image-tests
  slate-codegen-tests
)

option(SANDBOX_KERNEL_BENCHMARKS "Build native generated-kernel benchmarks" OFF)
option(SANDBOX_NATIVE_SOA_BENCHMARKS "Build standalone experimental SoA benchmarks" OFF)
option(SANDBOX_KERNEL_BENCHMARK_PLOTS "Generate native kernel benchmark plots" OFF)

add_subdirectory(third_party)
add_subdirectory(third_party/handmade_math)

set(SANDBOX_WITH_TRACY OFF)
if(UE_CONFIGURATION MATCHES "^(Debug|DebugGame|Development)$")
  set(SANDBOX_WITH_TRACY ON)

  set(TRACY_ENABLE ON)
  set(TRACY_ON_DEMAND ON)
  set(TRACY_ONLY_LOCALHOST ON)
  set(TRACY_NO_BROADCAST ON)
  set(TRACY_NO_CRASH_HANDLER ON)
  if(SANDBOX_WITH_UNREAL)
    set(TRACY_STATIC OFF)
    set(TRACY_MANUAL_LIFETIME ON)
  else()
    set(TRACY_STATIC ON)
    set(TRACY_MANUAL_LIFETIME OFF)
  endif()

  add_subdirectory(third_party/tracy EXCLUDE_FROM_ALL)
  set_property(TARGET TracyClient PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(TracyClient PRIVATE _ITERATOR_DEBUG_LEVEL=0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(TracyClient PRIVATE -Wno-format -Wno-unused-value)
  elseif(MSVC)
    target_compile_options(TracyClient PRIVATE /wd4477 /wd4552)
  endif()
  set_target_properties(TracyClient PROPERTIES OUTPUT_NAME SandboxTracyClient)

  if(SANDBOX_WITH_UNREAL)
    set_target_properties(TracyClient PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY
        "${PROJECT_SOURCE_DIR}/Binaries/Native/Tracy/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
      RUNTIME_OUTPUT_DIRECTORY
        "${PROJECT_SOURCE_DIR}/Binaries/Native/Tracy/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
    )
  endif()
endif()

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

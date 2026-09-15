option(SANDBOX_KERNEL_BENCHMARKS "Build native generated-kernel benchmarks" OFF)
option(SANDBOX_NATIVE_SOA_BENCHMARKS "Build standalone experimental SoA benchmarks" OFF)
option(SANDBOX_FRAME_MEMORY_LEVEL_BENCHMARK "Register the native frame-memory level benchmark" OFF)
option(SANDBOX_TELEMETRY_INTEGRATION_BENCHMARK
  "Register the native level telemetry integration benchmark" OFF)
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
  set(TRACY_STATIC ON)
  set(TRACY_MANUAL_LIFETIME OFF)

  find_package(Threads REQUIRED)
  add_subdirectory(third_party/tracy EXCLUDE_FROM_ALL)
  set_property(TARGET TracyClient PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(TracyClient PRIVATE _ITERATOR_DEBUG_LEVEL=0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(TracyClient PRIVATE -Wno-format -Wno-unused-value)
  elseif(MSVC)
    target_compile_options(TracyClient PRIVATE /wd4477 /wd4552)
  endif()
  set_target_properties(TracyClient PROPERTIES OUTPUT_NAME SandboxTracyClientStatic)

  if(SANDBOX_WITH_UNREAL)
    add_library(TracyClientUnreal SHARED)
    target_sources(TracyClientUnreal PRIVATE third_party/tracy/public/TracyClient.cpp)
    target_compile_features(TracyClientUnreal PUBLIC cxx_std_11)
    target_include_directories(TracyClientUnreal SYSTEM PUBLIC third_party/tracy/public)
    get_target_property(tracy_definitions TracyClient INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(tracy_libraries TracyClient INTERFACE_LINK_LIBRARIES)
    target_compile_definitions(TracyClientUnreal
      PUBLIC ${tracy_definitions} TRACY_MANUAL_LIFETIME TRACY_IMPORTS
      PRIVATE TRACY_EXPORTS _ITERATOR_DEBUG_LEVEL=0
    )
    target_link_libraries(TracyClientUnreal PUBLIC ${tracy_libraries})
    set_property(TARGET TracyClientUnreal PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(TracyClientUnreal PRIVATE -Wno-format -Wno-unused-value)
    elseif(MSVC)
      target_compile_options(TracyClientUnreal PRIVATE /wd4477 /wd4552)
    endif()
    set_target_properties(TracyClientUnreal PROPERTIES
      OUTPUT_NAME SandboxTracyClient
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
add_subdirectory(level_authoring)
add_subdirectory(simulation_benchmark)
add_subdirectory(image)
add_subdirectory(mesh_gen)

sandbox_target_precompile_common_headers(
  native-core
  native-simulation
  native-simulation-unreal
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
  native-level-authoring-tests
  native-simulation-tests
  native-simulation-benchmark-tests
  sandbox-image-tests
  slate-codegen-tests
)

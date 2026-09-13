cmake_minimum_required(VERSION 4.4.2)

include(cmake/machine_activity_integration.cmake)
sandbox_configure_machine_activity()

project(SandboxUnrealBuild LANGUAGES CXX)
sandbox_add_machine_activity_status_target()

enable_testing()

add_subdirectory(cmake/compiler_warnings)
add_subdirectory(native/third_party/handmade_math)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(SANDBOX_CODEGEN_ONLY "Configure only the standalone code generator" OFF)
option(SANDBOX_NATIVE_CORE_ONLY "Configure only the standalone native core library" OFF)
option(SANDBOX_NATIVE_MEMORY_ONLY "Configure only the standalone native memory library" OFF)
option(SANDBOX_NATIVE_SIMULATION_ONLY "Configure only the standalone native simulation library" OFF)
option(SANDBOX_NATIVE_MESH_ONLY "Configure only the standalone native mesh generator" OFF)
option(SANDBOX_NATIVE_S7_ONLY "Configure only the standalone native S7 library" OFF)
option(SANDBOX_KERNEL_BENCHMARKS "Build native generated-kernel benchmarks" OFF)
option(SANDBOX_NATIVE_SOA_BENCHMARKS "Build standalone experimental SoA benchmarks" OFF)
option(SANDBOX_KERNEL_BENCHMARK_PLOTS "Generate native kernel benchmark plots" OFF)

set(CACHE{UE_PLATFORM}
  TYPE STRING
  HELP "Unreal target platform passed to UnrealBuildTool"
  VALUE Win64
)

set(CACHE{UE_CONFIGURATION}
  TYPE STRING
  HELP "Unreal target configuration and native artifact configuration"
  VALUE Development
)
set_property(CACHE UE_CONFIGURATION PROPERTY STRINGS
  Debug
  DebugGame
  Development
  Shipping
  Test
)

if(SANDBOX_NATIVE_CORE_ONLY)
  add_subdirectory(native/core)
  return()
endif()

if(SANDBOX_NATIVE_MEMORY_ONLY)
  add_subdirectory(native/memory)
  return()
endif()

if(SANDBOX_NATIVE_SIMULATION_ONLY)
  add_subdirectory(native/simulation)
  return()
endif()

if(SANDBOX_NATIVE_MESH_ONLY)
  add_subdirectory(native/mesh_gen)
  return()
endif()

if(SANDBOX_NATIVE_S7_ONLY)
  add_subdirectory(native/s7)
  return()
endif()

add_subdirectory(native/sbx_mimalloc)
add_subdirectory(native/lispb)

if(SANDBOX_CODEGEN_ONLY)
  return()
endif()

add_subdirectory(native/memory)
add_subdirectory(native/s7)
add_subdirectory(native/core)
add_subdirectory(native/simulation)

add_subdirectory(Scripts)

add_subdirectory(native/image)
add_subdirectory(native/mesh_gen)

add_subdirectory(lispb/materials)
add_subdirectory(Plugins/SpaceGame)

add_subdirectory(unreal)

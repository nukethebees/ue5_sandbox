cmake_minimum_required(VERSION 4.4.2)



project(SandboxUnrealBuild LANGUAGES CXX)
enable_testing()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_UNITY_BUILD)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(cmake/compiler_detection.cmake)
include(cmake/llvm_tools.cmake)
include(cmake/rust_host_tools.cmake)
include(cmake/ue_cache_vars.cmake)

ioj_is_msvc_frontend(IOJ_MSVC_FRONTEND)
ioj_is_clang_cl(IOJ_IS_CLANG_CL)

option(IOJ_WITH_UNREAL "Configure Unreal Engine integration and targets" ON)
option(IOJ_ENABLE_ASAN "Instrument native targets with AddressSanitizer" OFF)
option(IOJ_ENABLE_CLANG_TIDY "Run clang-tidy for first-party native targets" OFF)
option(IOJ_BUILD_DEVELOPER_TOOLS "Configure developer applications and their test/build dependencies" ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory(cmake)

if(IOJ_ENABLE_ASAN)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

set(IOJ_HAS_PYTHON ${Python3_Interpreter_FOUND})

sandbox_add_rust_host_tool(
  benchmark-tools
  IOJ_BENCHMARK_TOOLS
)

add_subdirectory(native)
add_subdirectory(tools/jobserver)
add_subdirectory(Scripts)
add_subdirectory(tools)
add_subdirectory(lispb/materials)
add_subdirectory(unreal)

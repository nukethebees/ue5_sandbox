cmake_minimum_required(VERSION 4.4.2)

include(cmake/jobserver_integration.cmake)
sandbox_configure_jobserver()

project(SandboxUnrealBuild LANGUAGES CXX)
enable_testing()

sandbox_add_jobserver_status_target()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_UNITY_BUILD)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(cmake/compiler_detection.cmake)
include(cmake/dotnet_host_tools.cmake)
include(cmake/ue_cache_vars.cmake)

ioj_is_msvc_frontend(IOJ_MSVC_FRONTEND)
ioj_is_clang_cl(IOJ_IS_CLANG_CL)

add_subdirectory(cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(IOJ_HAS_PYTHON ${Python3_Interpreter_FOUND})

option(IOJ_WITH_UNREAL "Configure Unreal Engine integration and targets" ON)
option(IOJ_ENABLE_ASAN "Instrument native targets with AddressSanitizer" OFF)
option(IOJ_ENABLE_CLANG_TIDY "Run clang-tidy for first-party native targets" OFF)

if(IOJ_ENABLE_ASAN)
  sandbox_enable_address_sanitizer()
endif()

sandbox_add_dotnet_host_tool(
  benchmark-tools-host
  IOJ_BENCHMARK_TOOLS
  "${PROJECT_SOURCE_DIR}/tools/BenchmarkTools/BenchmarkTools.csproj"
)

add_subdirectory(native)
add_subdirectory(tools/rust)
add_subdirectory(tools/jobserver)
add_subdirectory(Scripts)
add_subdirectory(tools)
add_subdirectory(lispb/materials)
add_subdirectory(Plugins/SpaceGame)
add_subdirectory(unreal)

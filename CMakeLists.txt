cmake_minimum_required(VERSION 4.4.2)

include(cmake/jobserver_integration.cmake)
sandbox_configure_jobserver()

project(SandboxUnrealBuild LANGUAGES CXX)
sandbox_add_jobserver_status_target()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_UNITY_BUILD)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(cmake/compiler_detection.cmake)
ioj_is_msvc_frontend(IOJ_MSVC_FRONTEND)
ioj_is_clang_cl(IOJ_IS_CLANG_CL)

enable_testing()

include(cmake/dotnet_host_tools.cmake)
add_custom_target(csharp-host-tools)

add_subdirectory(cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(IOJ_WITH_UNREAL "Configure Unreal Engine integration and targets" ON)
option(IOJ_WITH_ASAN "Instrument native targets with AddressSanitizer" OFF)
option(IOJ_WITH_CLANG_TIDY "Run clang-tidy for first-party native targets" OFF)

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

if(IOJ_WITH_ASAN)
  include(cmake/address_sanitizer.cmake)
  sandbox_enable_address_sanitizer()
endif()

add_subdirectory(cmake/clang_tidy)

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

if(IOJ_WITH_UNREAL)
  add_subdirectory(unreal)
endif()

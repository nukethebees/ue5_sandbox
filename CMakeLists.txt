cmake_minimum_required(VERSION 4.4.2)

include(cmake/jobserver_integration.cmake)
sandbox_configure_jobserver()

project(SandboxUnrealBuild LANGUAGES CXX)
sandbox_add_jobserver_status_target()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_UNITY_BUILD)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(cmake/precompiled_headers.cmake)

enable_testing()

include(cmake/staged_csharp_tools.cmake)

add_subdirectory(cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(SANDBOX_WITH_UNREAL "Configure Unreal Engine integration and targets" ON)
option(SANDBOX_WITH_ASAN "Instrument native targets with AddressSanitizer" OFF)
option(SANDBOX_WITH_CLANG_TIDY "Run clang-tidy for first-party native targets" OFF)

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

if(SANDBOX_WITH_ASAN)
  include(cmake/address_sanitizer.cmake)
  sandbox_enable_address_sanitizer()
endif()

add_subdirectory(cmake/clang_tidy)

add_subdirectory(native)
add_subdirectory(tools/jobserver)
add_subdirectory(Scripts)
add_subdirectory(tools)
add_subdirectory(lispb/materials)
add_subdirectory(Plugins/SpaceGame)

if(SANDBOX_WITH_UNREAL)
  add_subdirectory(unreal)
endif()

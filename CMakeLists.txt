cmake_minimum_required(VERSION 4.4.2)

include(cmake/machine_activity_integration.cmake)
sandbox_configure_machine_activity()

project(SandboxUnrealBuild LANGUAGES CXX)
sandbox_add_machine_activity_status_target()

if(CMAKE_UNITY_BUILD)
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

include(cmake/precompiled_headers.cmake)

enable_testing()

add_subdirectory(cmake)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(SANDBOX_WITH_UNREAL "Configure Unreal Engine integration and targets" ON)
option(SANDBOX_WITH_ASAN "Instrument native targets with AddressSanitizer" OFF)

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

add_subdirectory(native)
add_subdirectory(Scripts)
add_subdirectory(lispb/materials)
add_subdirectory(Plugins/SpaceGame)

if(SANDBOX_WITH_UNREAL)
  add_subdirectory(unreal)
endif()

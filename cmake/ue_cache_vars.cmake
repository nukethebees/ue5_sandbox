include_guard(GLOBAL)

option(IOJ_ENABLE_UBA_CACHE "Start and use the local Unreal Build Accelerator cache" ON)
set(IOJ_UBA_CACHE_CAPACITY_GB 100 CACHE STRING "Local UBA cache capacity in GB")

set(CACHE{UE_ROOT}
  TYPE PATH
  HELP "Root directory of the Unreal Engine installation"
  VALUE "$ENV{UE_ROOT}"
)

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

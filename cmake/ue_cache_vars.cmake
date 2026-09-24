include_guard(GLOBAL)

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
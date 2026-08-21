cmake_minimum_required(VERSION 4.3)

project(SandboxUnrealBuild NONE)

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
    HELP "Unreal build configuration passed to UnrealBuildTool"
    VALUE Development
)
set_property(CACHE UE_CONFIGURATION PROPERTY STRINGS
    Debug
    DebugGame
    Development
    Shipping
    Test
)

if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR
        "This initial CMake build layer supports Windows hosts only. "
        "It invokes Unreal's Engine/Build/BatchFiles/Build.bat script.")
endif()

if(NOT UE_ROOT)
    message(FATAL_ERROR
        "UE_ROOT is required. Set the UE_ROOT environment variable, pass "
        "-DUE_ROOT=<engine-root>, or set UE_ROOT in CMakeUserPresets.json.")
endif()

cmake_path(ABSOLUTE_PATH UE_ROOT NORMALIZE OUTPUT_VARIABLE UE_ROOT_ABSOLUTE)

cmake_path(APPEND UE_ROOT_ABSOLUTE Engine Build BatchFiles Build.bat
    OUTPUT_VARIABLE UE_BUILD_SCRIPT
)
if(NOT EXISTS "${UE_BUILD_SCRIPT}")
    message(FATAL_ERROR
        "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
        "Expected to find '${UE_BUILD_SCRIPT}'.")
endif()

set(SANDBOX_UPROJECT "${CMAKE_CURRENT_SOURCE_DIR}/Sandbox.uproject")

function(add_unreal_target target_name unreal_target)
    add_custom_target(${target_name}
        COMMAND "${UE_BUILD_SCRIPT}" ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION}
            "-Project=${SANDBOX_UPROJECT}" -WaitMutex
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building ${unreal_target} ${UE_PLATFORM} ${UE_CONFIGURATION} through UnrealBuildTool"
        USES_TERMINAL
        VERBATIM
    )
endfunction()

add_unreal_target(editor SandboxEditor)
add_unreal_target(game Sandbox)

cmake_minimum_required(VERSION 4.4.2)

project(SandboxUnrealBuild NONE)

enable_testing()

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

if(UE_CONFIGURATION STREQUAL "Development")
    set(UE_EDITOR_CMD_NAME UnrealEditor-Cmd.exe)
else()
    set(UE_EDITOR_CMD_NAME "UnrealEditor-Win64-${UE_CONFIGURATION}-Cmd.exe")
endif()

cmake_path(APPEND UE_ROOT_ABSOLUTE Engine Binaries Win64 "${UE_EDITOR_CMD_NAME}"
    OUTPUT_VARIABLE UE_EDITOR_CMD_EXE
)
if(NOT EXISTS "${UE_EDITOR_CMD_EXE}")
    message(FATAL_ERROR
        "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
        "Expected to find '${UE_EDITOR_CMD_EXE}'.")
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

function(add_low_level_test_suite target_name unreal_target test_name_prefix test_label)
    add_unreal_target(${target_name} ${unreal_target})

    set(test_executable
        "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/${UE_PLATFORM}/${unreal_target}/${unreal_target}-${UE_PLATFORM}-${UE_CONFIGURATION}.exe"
    )

    discover_tests(
        COMMAND "${test_executable}"
        DISCOVERY_ARGS --list-tests --reporter JSON
        DISCOVERY_MATCH "^[ \\t]*\\\"name\\\": \\\"(${test_name_prefix}\\.[^\\\"]+)\\\",$"
        TEST_NAME "\\1"
        TEST_ARGS "\\1"
        TEST_PROPERTIES
            LABELS ${test_label}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

add_unreal_target(editor SandboxEditor)
add_unreal_target(game Sandbox)
add_low_level_test_suite(core-tests SandboxCoreTests SandboxCore low-level-core)
add_low_level_test_suite(native-tests SandboxNativeTests SandboxNative low-level-native)

add_custom_target(dev-core 
  DEPENDS 
    editor 
    core-tests 
    native-tests
)

add_custom_target(resave-assets
    COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
        -run=ResavePackages
        -projectonly
        -fixupredirects
        -unattended
    DEPENDS editor
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Resaving project assets and fixing redirectors"
    USES_TERMINAL
    VERBATIM
)

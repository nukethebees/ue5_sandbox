cmake_minimum_required(VERSION 4.4.2)

project(SandboxUnrealBuild LANGUAGES CXX)

enable_testing()

option(SANDBOX_CODEGEN_ONLY "Configure only the standalone code generator" OFF)

add_subdirectory(Codegen)

if(SANDBOX_CODEGEN_ONLY)
  return()
endif()

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

set(CACHE{SANDBOX_TEST_SUITE}
  TYPE STRING
  HELP "Low-level test suite registered with CTest"
  VALUE default
)
set_property(CACHE SANDBOX_TEST_SUITE PROPERTY STRINGS
  default
  benchmark
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

cmake_path(APPEND UE_ROOT_ABSOLUTE Engine Build BatchFiles GenerateProjectFiles.bat
  OUTPUT_VARIABLE UE_GENERATE_PROJECT_FILES_SCRIPT
)
if(NOT EXISTS "${UE_GENERATE_PROJECT_FILES_SCRIPT}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find '${UE_GENERATE_PROJECT_FILES_SCRIPT}'.")
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
set(SANDBOX_LOCAL_DDC_DIR "${CMAKE_CURRENT_BINARY_DIR}/local-derived-data-cache")
file(MAKE_DIRECTORY "${SANDBOX_LOCAL_DDC_DIR}")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(format-code
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/format-cpp.py" --changed
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Formatting Git-changed project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-all-code
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/format-cpp.py"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Formatting all project source files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(generate-project-files
  COMMAND "${UE_GENERATE_PROJECT_FILES_SCRIPT}"
    "-Project=${SANDBOX_UPROJECT}"
    -Game
    -WaitMutex
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating Visual Studio project files for ${SANDBOX_UPROJECT}"
  USES_TERMINAL
  VERBATIM
)

include(cmake/unreal.cmake)
include(cmake/catch2.cmake)

add_unreal_target(editor SandboxEditor)
add_unreal_target(game Sandbox)
add_dependencies(editor check-generated-code)
add_dependencies(game check-generated-code)

add_custom_target(generate-lab-images
  COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
    -run=GenerateSandboxImages
    "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
    -ddc=NoZenLocalFallback
    -unattended
    -nop4
    -nosplash
    -nullrhi
    -nosound
    -stdout
  DEPENDS editor
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Regenerating SandboxImages Lab PNGs"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(generate-s7lab-assets
  COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
    -run=GenerateS7LabAssets
    "-LocalDataCachePath=${SANDBOX_LOCAL_DDC_DIR}"
    -ddc=NoZenLocalFallback
    -unattended
    -nop4
    -nosplash
    -nullrhi
    -nosound
    -stdout
  DEPENDS editor
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating S7Lab Editor Utility Widget assets"
  USES_TERMINAL
  VERBATIM
)

set(SANDBOX_UNIT_AUTOMATION_FILTERS
  Sandbox.enum
  Sandbox.fighters
  Sandbox.geometry
  Sandbox.levels
  Sandbox.UnitTests
  Sandbox.USFPathValidation
  SandboxShaders.UnitTests
  SandboxCoreEngine.UnitTests
  SandboxGpuTutorials.UnitTests
  SandboxImages.UnitTests
  S7Lab.UnitTests
  SandboxUI.UnitTests
)
set(SANDBOX_LEVEL_AUTOMATION_FILTERS
  Sandbox.LevelTests
  SandboxCoreEngine.LevelTests
)

add_unreal_automation_test(
  Sandbox.UnitTests
  FILTERS ${SANDBOX_UNIT_AUTOMATION_FILTERS}
  LABELS unit
)
add_unreal_automation_test(
  Sandbox.LevelTests
  FILTERS ${SANDBOX_LEVEL_AUTOMATION_FILTERS}
  LABELS level
)
add_unreal_automation_test(
  Sandbox.AllAutomationTests
  FILTERS
    ${SANDBOX_UNIT_AUTOMATION_FILTERS}
    ${SANDBOX_LEVEL_AUTOMATION_FILTERS}
  LABELS all
)

add_custom_target(dev-core
  DEPENDS
    editor
    codegen-tests
)

if(SANDBOX_TEST_SUITE STREQUAL "benchmark")
  add_low_level_test_suite(benchmarks SandboxCoreBenchmarks SandboxCore benchmark)
  add_dependencies(benchmarks check-generated-code)
elseif(SANDBOX_TEST_SUITE STREQUAL "default")
  add_low_level_test_suite(core-tests SandboxCoreTests SandboxCore low-level-core)
  add_low_level_test_suite(native-tests SandboxNativeTests SandboxNative low-level-native)

  add_dependencies(core-tests check-generated-code)
  add_dependencies(native-tests check-generated-code)

  add_dependencies(dev-core
    core-tests
    native-tests
  )
else()
  message(FATAL_ERROR
    "SANDBOX_TEST_SUITE must be one of: default, benchmark. "
    "Got '${SANDBOX_TEST_SUITE}'."
  )
endif()

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

add_test(
  NAME SandboxShaders.ShaderSmoke
  COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
    "-ExecCmds=Automation RunTests StartsWith:SandboxShaders.ShaderSmoke; Quit"
    -AllowCommandletRendering
    -unattended
    -DDC-ForceMemoryCache
    -nop4
    -nosplash
    -nosound
    -stdout
)
set_tests_properties(SandboxShaders.ShaderSmoke PROPERTIES
  LABELS all
  TIMEOUT 900
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(gpu-tutorial-shader-tests
  COMMAND "${UE_EDITOR_CMD_EXE}" "${SANDBOX_UPROJECT}"
    "-ExecCmds=Automation RunTests StartsWith:SandboxGpuTutorials.UnitTests; Quit"
    -unattended
    -nop4
    -nosplash
    -nosound
    -DDC-ForceMemoryCache
    -stdout
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Compiling and validating SandboxGpuTutorials materials with the graphics RHI"
  VERBATIM
)

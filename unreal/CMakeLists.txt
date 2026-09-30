if(NOT IOJ_WITH_UNREAL)
  return()
endif()

set(CACHE{IOJ_SPACE_GAME_TEST_TIME_SCALE}
  TYPE STRING
  HELP "Optional positive time scale for level-backed SpaceGame automation"
  VALUE ""
)

if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR
    "This initial CMake build layer supports Windows hosts only. "
    "It invokes Unreal's Engine/Build/BatchFiles build scripts.")
endif()

if(NOT UE_ROOT)
  message(FATAL_ERROR
    "UE_ROOT is required. Set the UE_ROOT environment variable, pass "
    "-DUE_ROOT=<engine-root>, or set UE_ROOT in CMakeUserPresets.json.")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/unreal_paths.cmake")
cmake_path(ABSOLUTE_PATH UE_ROOT BASE_DIRECTORY "${PROJECT_SOURCE_DIR}" NORMALIZE
  OUTPUT_VARIABLE UE_ROOT_ABSOLUTE)
sandbox_get_unreal_editor_names(UE_EDITOR_NAME UE_EDITOR_CMD_NAME "${UE_CONFIGURATION}")
sandbox_configure_unreal_engine_paths(UE "${UE_ROOT_ABSOLUTE}"
  "${UE_EDITOR_NAME}" "${UE_EDITOR_CMD_NAME}")

if(NOT EXISTS "${UE_BUILD_SCRIPT}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find Engine/Build/BatchFiles/RunUBT.bat or Build.bat.")
endif()

if(NOT EXISTS "${UE_RUN_UAT_SCRIPT}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find '${UE_RUN_UAT_SCRIPT}'.")
endif()

if(NOT EXISTS "${UE_EDITOR_EXE}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find '${UE_EDITOR_EXE}'.")
endif()

if(NOT EXISTS "${UE_EDITOR_CMD_EXE}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find '${UE_EDITOR_CMD_EXE}'.")
endif()

if(NOT EXISTS "${UE_UNREAL_PAK_EXE}")
  message(FATAL_ERROR
    "UE_ROOT='${UE_ROOT_ABSOLUTE}' is not a usable Unreal Engine installation. "
    "Expected to find '${UE_UNREAL_PAK_EXE}'.")
endif()

set(IOJ_UPROJECT "${PROJECT_SOURCE_DIR}/Sandbox.uproject")
set(IOJ_LOCAL_DDC_DIR "${PROJECT_BINARY_DIR}/local-derived-data-cache")
if(DEFINED ENV{IOJ_GAME_ARTIFACT_ROOT} AND
    NOT "$ENV{IOJ_GAME_ARTIFACT_ROOT}" STREQUAL "")
  set(sandbox_game_artifact_root_default "$ENV{IOJ_GAME_ARTIFACT_ROOT}")
else()
  set(sandbox_game_artifact_root_default "${PROJECT_SOURCE_DIR}/out/game-package")
endif()
set(IOJ_GAME_ARTIFACT_ROOT "${sandbox_game_artifact_root_default}" CACHE PATH
  "Root directory for cooked, staged, and archived game artifacts")
sandbox_configure_game_artifact_paths(IOJ_GAME
  "${IOJ_GAME_ARTIFACT_ROOT}" "${PROJECT_SOURCE_DIR}"
  "${UE_PLATFORM}" "${UE_CONFIGURATION}")
set(IOJ_GAME_ARTIFACT_ROOT_ABSOLUTE "${IOJ_GAME_ROOT}")
set(IOJ_GAME_PLATFORM_ROOT "${IOJ_GAME_ROOT}/${UE_PLATFORM}")
set(IOJ_GAME_CONFIGURATION_ROOT
  "${IOJ_GAME_PLATFORM_ROOT}/${UE_CONFIGURATION}")
sandbox_add_rust_host_tool(
  game-package-tools
  IOJ_GAME_PACKAGE_TOOLS
)
file(MAKE_DIRECTORY "${IOJ_LOCAL_DDC_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/cmake/unreal-paths.json.in"
  "${PROJECT_BINARY_DIR}/unreal-paths.json" @ONLY)

include("${PROJECT_SOURCE_DIR}/cmake/unreal.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/unreal_packaging.cmake")

add_subdirectory(build)

set(IOJ_UNIT_AUTOMATION_FILTERS
  Sandbox.enum
  Sandbox.fighters
  Sandbox.geometry
  Sandbox.levels
  Sandbox.UnitTests
  SandboxShaders.UnitTests
  SandboxCoreEngine.UnitTests
  SandboxGpuTutorials.UnitTests
  SandboxISMC.UnitTests
  SandboxMesh.UnitTests
)
set(IOJ_LEVEL_AUTOMATION_FILTERS
  Sandbox.LevelTests
  SandboxCoreEngine.LevelTests
)

add_unreal_automation_test(
  Sandbox.ExportSimulationFixture
  FILTERS Sandbox.FixtureTools.SimulationFixtureExport
  LABELS fixture-tools
)

add_unreal_automation_test(
  Sandbox.UnitTests
  FILTERS ${IOJ_UNIT_AUTOMATION_FILTERS}
  LABELS unit
)
add_unreal_automation_test(
  Sandbox.S7AuthoringTests
  FILTERS
    Sandbox.UnitTests.S7LevelAuthoring
    Sandbox.UnitTests.SpaceGameLevelConfig.CollisionGridDimensionOverridesKeepUnspecifiedAssetValues
  LABELS s7-authoring
)
add_unreal_automation_test(
  SandboxISMC.ModuleLoadSmoke
  FILTERS SandboxISMC.ModuleLoadSmoke
  LABELS ismc-module-smoke
)
add_unreal_automation_test(
  SandboxISMC.UnitTests
  FILTERS SandboxISMC.UnitTests Sandbox.UnitTests.LaserPresentation
  LABELS unit ismc
)
add_unreal_automation_test(
  Sandbox.LevelTests
  FILTERS ${IOJ_LEVEL_AUTOMATION_FILTERS}
  LABELS level
)
add_unreal_automation_test(
  Sandbox.InputSmokeTests
  FILTERS Sandbox.LevelTests.PlayerInputSmoke
  LABELS input-smoke
)
add_unreal_automation_test(
  Sandbox.InputUnitTests
  FILTERS
    Sandbox.UnitTests.CanonicalShipInput
    Sandbox.UnitTests.GameSettingsEditState
    Sandbox.UnitTests.PlayerController
    Sandbox.UnitTests.ControlContextTransitions
  LABELS input-unit
)
add_unreal_automation_test(
  Sandbox.AllAutomationTests
  FILTERS
    ${IOJ_UNIT_AUTOMATION_FILTERS}
    ${IOJ_LEVEL_AUTOMATION_FILTERS}
  LABELS all
)
add_unreal_automation_test(
  USFLoader.UnitTests
  FILTERS USFLoader.UnitTests
  LABELS usfloader
)

sandbox_make_automation_exec_commands(ismc_render_test_commands
  "StartsWith:SandboxISMC.RenderTests")
add_unreal_editor_test(SandboxISMC.RenderTests
  ARGUMENTS
    "${ismc_render_test_commands}"
    -RenderOffscreen
  LABELS level all
  TIMEOUT 900
)
sandbox_set_automation_test_properties(SandboxISMC.RenderTests)

sandbox_make_automation_exec_commands(spark_render_test_commands
  "StartsWith:Sandbox.SparkRenderTests")
add_unreal_editor_test(Sandbox.SparkRenderTests
  ARGUMENTS
    "${spark_render_test_commands}"
    -RenderOffscreen
  LABELS level all
  TIMEOUT 900
)
sandbox_set_automation_test_properties(Sandbox.SparkRenderTests)

add_custom_target(unreal-unit-tests
  DEPENDS
    editor
    native-tests
)
set_property(TARGET unreal-unit-tests PROPERTY FOLDER "Sandbox/Unreal/Tests")

add_custom_target(dev-core
  DEPENDS
    editor
    native-tests
)

add_unreal_editor_target(validate-ui-glow-material-unreal
  UNATTENDED
  ARGUMENTS
    -run=MaterialSynth
    "-Artifact=${UI_GLOW_MATERIAL_ARTIFACT}"
    -Validate -unattended -nop4 -nosplash -nullrhi -nosound -stdout
  DEPENDS editor compile-ui-glow-material
  COMMENT "Validating the UI glow material in Unreal"
  FOLDER "Sandbox/Unreal/Maintenance"
)

sandbox_make_automation_exec_commands(material_compilation_test_commands
  "StartsWith:SandboxShaders.MaterialCompilation")
add_unreal_editor_test(SandboxShaders.MaterialCompilation
  NO_LOCAL_DDC
  ARGUMENTS
    "${material_compilation_test_commands}"
    -AllowCommandletRendering
    -DDC-ForceMemoryCache
  LABELS focused-material all
  TIMEOUT 900
)
sandbox_set_automation_test_properties(SandboxShaders.MaterialCompilation)

sandbox_make_automation_exec_commands(included_function_compilation_test_commands
  "StartsWith:USFLoader.IncludedFunctionCompilation")
add_unreal_editor_test(USFLoader.IncludedFunctionCompilation
  NO_LOCAL_DDC
  ARGUMENTS
    "${included_function_compilation_test_commands}"
    -AllowCommandletRendering
    -DDC-ForceMemoryCache
  LABELS usfloader
  TIMEOUT 900
)
sandbox_set_automation_test_properties(USFLoader.IncludedFunctionCompilation)

sandbox_make_automation_exec_commands(material_synth_test_commands
  "SandboxEditor.MaterialSynth")
add_unreal_editor_test(SandboxEditor.MaterialSynth
  NO_LOCAL_DDC
  ARGUMENTS
    "${material_synth_test_commands}"
    -RenderOffscreen
    -AllowCommandletRendering
    -DDC-ForceMemoryCache
  LABELS "material-synth;all"
  TIMEOUT 900
)
sandbox_set_automation_test_properties(SandboxEditor.MaterialSynth)

sandbox_make_automation_exec_commands(gpu_tutorial_shader_test_commands
  "StartsWith:SandboxGpuTutorials.UnitTests")
add_custom_target(gpu-tutorial-shader-tests
  COMMAND "${UE_EDITOR_CMD_EXE}" "${IOJ_UPROJECT}"
    "${gpu_tutorial_shader_test_commands}"
    -unattended
    -nop4
    -nosplash
    -nosound
    -DDC-ForceMemoryCache
    -stdout
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Compiling and validating SandboxGpuTutorials materials with the graphics RHI"
  VERBATIM
)

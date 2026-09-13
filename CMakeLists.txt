cmake_minimum_required(VERSION 4.4.2)

include(cmake/machine_activity_integration.cmake)
sandbox_configure_machine_activity()

project(SandboxUnrealBuild LANGUAGES CXX)
sandbox_add_machine_activity_status_target()

enable_testing()

add_subdirectory(cmake/compiler_warnings)
add_subdirectory(native/third_party/handmade_math)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(SANDBOX_CODEGEN_ONLY "Configure only the standalone code generator" OFF)
option(SANDBOX_NATIVE_CORE_ONLY "Configure only the standalone native core library" OFF)
option(SANDBOX_NATIVE_MEMORY_ONLY "Configure only the standalone native memory library" OFF)
option(SANDBOX_NATIVE_SIMULATION_ONLY "Configure only the standalone native simulation library" OFF)
option(SANDBOX_NATIVE_MESH_ONLY "Configure only the standalone native mesh generator" OFF)
option(SANDBOX_NATIVE_S7_ONLY "Configure only the standalone native S7 library" OFF)
option(SANDBOX_KERNEL_BENCHMARKS "Build native generated-kernel benchmarks" OFF)
option(SANDBOX_NATIVE_SOA_BENCHMARKS "Build standalone experimental SoA benchmarks" OFF)
option(SANDBOX_KERNEL_BENCHMARK_PLOTS "Generate native kernel benchmark plots" OFF)

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

if(SANDBOX_NATIVE_CORE_ONLY)
  add_subdirectory(native/core)
  return()
endif()

if(SANDBOX_NATIVE_MEMORY_ONLY)
  add_subdirectory(native/memory)
  return()
endif()

if(SANDBOX_NATIVE_SIMULATION_ONLY)
  add_subdirectory(native/simulation)
  return()
endif()

if(SANDBOX_NATIVE_MESH_ONLY)
  add_subdirectory(native/mesh_gen)
  return()
endif()

if(SANDBOX_NATIVE_S7_ONLY)
  add_subdirectory(native/s7)
  return()
endif()

add_subdirectory(native/sbx_mimalloc)
add_subdirectory(native/lispb)

if(SANDBOX_CODEGEN_ONLY)
  return()
endif()

add_subdirectory(native/memory)
add_subdirectory(native/s7)
add_subdirectory(native/core)
add_subdirectory(native/simulation)

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

add_subdirectory(native/image)
add_subdirectory(native/mesh_gen)

set(UI_GLOW_MATERIAL_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb"
)
set(UI_GLOW_MATERIAL_ARTIFACT
  "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen/UiGlowComposite.smat"
)
set(UI_GLOW_MATERIAL_DEPFILE
  "${CMAKE_CURRENT_BINARY_DIR}/UiGlowComposite.smat.d"
)
add_custom_command(
  OUTPUT "${UI_GLOW_MATERIAL_ARTIFACT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen"
  COMMAND $<TARGET_FILE:lispb> generate
    --project "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target ui-glow-material
    --depfile "${UI_GLOW_MATERIAL_DEPFILE}"
  DEPENDS lispb "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb" "${UI_GLOW_MATERIAL_SOURCE}"
  DEPFILE "${UI_GLOW_MATERIAL_DEPFILE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Compiling UiGlow material IR"
  VERBATIM
)
add_custom_target(compile-ui-glow-material DEPENDS "${UI_GLOW_MATERIAL_ARTIFACT}")

set(WORLD_SOFT_TARGET_MATERIAL_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/SoftTargetWorld.lispb"
)
set(WORLD_SOFT_TARGET_MATERIAL_ARTIFACT
  "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen/SoftTargetWorld.smat"
)
set(WORLD_SOFT_TARGET_MATERIAL_DEPFILE
  "${CMAKE_CURRENT_BINARY_DIR}/SoftTargetWorld.smat.d"
)
add_custom_command(
  OUTPUT "${WORLD_SOFT_TARGET_MATERIAL_ARTIFACT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen"
  COMMAND $<TARGET_FILE:lispb> generate
    --project "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target world-soft-target-material
    --depfile "${WORLD_SOFT_TARGET_MATERIAL_DEPFILE}"
  DEPENDS lispb "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb" "${WORLD_SOFT_TARGET_MATERIAL_SOURCE}"
  DEPFILE "${WORLD_SOFT_TARGET_MATERIAL_DEPFILE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Compiling world soft-target material IR"
  VERBATIM
)
add_custom_target(compile-world-soft-target-material
  DEPENDS "${WORLD_SOFT_TARGET_MATERIAL_ARTIFACT}"
)

set(MIGRATED_MATERIAL_ARTIFACTS)
function(add_migrated_material_ir name target source artifact_name)
  set(source_path "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  set(artifact_path
    "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen/${artifact_name}.smat"
  )
  set(depfile_path "${CMAKE_CURRENT_BINARY_DIR}/${artifact_name}.smat.d")
  add_custom_command(
    OUTPUT "${artifact_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/Intermediate/MaterialGen"
    COMMAND $<TARGET_FILE:lispb> generate
      --project "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb"
      --build-root "${CMAKE_BINARY_DIR}"
      --target "${target}"
      --depfile "${depfile_path}"
    DEPENDS lispb "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb" "${source_path}"
    DEPFILE "${depfile_path}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Compiling ${name} material IR"
    VERBATIM
  )
  list(APPEND MIGRATED_MATERIAL_ARTIFACTS "${artifact_path}")
  set(MIGRATED_MATERIAL_ARTIFACTS "${MIGRATED_MATERIAL_ARTIFACTS}" PARENT_SCOPE)
endfunction()

set(SHADER_MATERIAL_SOURCE_ROOT
  "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials"
)
add_migrated_material_ir("radar display" radar-display-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/RadarDisplay.lispb" RadarDisplay)
add_migrated_material_ir("energy shield" energy-shield-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/EnergyShield.lispb" EnergyShield)
add_migrated_material_ir("space energy field" space-energy-field-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/SpaceEnergyFieldDisplay.lispb" SpaceEnergyField)
add_migrated_material_ir("glowing cross" glowing-cross-material
  "Source/Sandbox/materials/GlowingCross.lispb" GlowingCross)
add_migrated_material_ir("vertex ripple" vertex-ripple-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/VertexRipple.lispb" VertexRipple)
add_migrated_material_ir("construction spawn" construction-spawn-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/ConstructionSpawn.lispb" ConstructionSpawn)
add_migrated_material_ir("planet atmosphere" planet-atmosphere-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/PlanetAtmosphere.lispb" PlanetAtmosphere)
add_migrated_material_ir("planet surface" planet-surface-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/PlanetSurface.lispb" PlanetSurface)
add_migrated_material_ir("tactical scan" tactical-scan-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/TacticalScan.lispb" TacticalScan)
add_migrated_material_ir("celestial analytic" celestial-analytic-material
  "Plugins/SandboxShaders/Source/SandboxCelestials/Private/materials/CelestialAnalytic.lispb"
  CelestialAnalytic)
add_custom_target(compile-migrated-materials DEPENDS ${MIGRATED_MATERIAL_ARTIFACTS})

add_custom_target(check-space-game-layers
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/check_space_game_layers.py"
  COMMENT "Checking SpaceGame module boundaries"
  VERBATIM
)
add_test(NAME SpaceGame.Architecture
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/check_space_game_layers.py")
add_test(NAME SpaceGame.ArchitectureChecker
  COMMAND "${Python3_EXECUTABLE}" -m unittest Scripts.test_space_game_layers)
set_tests_properties(
  SpaceGame.Architecture
  SpaceGame.ArchitectureChecker
  PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_custom_target(validate-ui-glow-material
  COMMAND $<TARGET_FILE:lispb> validate
    --project "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target ui-glow-material
  DEPENDS lispb
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(validate-world-soft-target-material
  COMMAND $<TARGET_FILE:lispb> validate
    --project "${CMAKE_CURRENT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target world-soft-target-material
  DEPENDS lispb
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)

add_subdirectory(unreal)

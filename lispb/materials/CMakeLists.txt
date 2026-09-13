function(add_material_ir name target source artifact_name artifacts_variable)
  set(source_path "${PROJECT_SOURCE_DIR}/${source}")
  set(artifact_path
    "${PROJECT_SOURCE_DIR}/Intermediate/MaterialGen/${artifact_name}.smat"
  )
  set(depfile_path "${PROJECT_BINARY_DIR}/${artifact_name}.smat.d")
  add_custom_command(
    OUTPUT "${artifact_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "${PROJECT_SOURCE_DIR}/Intermediate/MaterialGen"
    COMMAND $<TARGET_FILE:lispb> generate
      --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
      --build-root "${CMAKE_BINARY_DIR}"
      --target "${target}"
      --depfile "${depfile_path}"
    DEPENDS lispb "${PROJECT_SOURCE_DIR}/lispb/project.lispb" "${source_path}"
    DEPFILE "${depfile_path}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Compiling ${name} material IR"
    VERBATIM
  )
  list(APPEND ${artifacts_variable} "${artifact_path}")
  set(${artifacts_variable} "${${artifacts_variable}}" PARENT_SCOPE)
endfunction()

add_material_ir("UiGlow" ui-glow-material
  "Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb"
  UiGlowComposite UI_GLOW_MATERIAL_ARTIFACT)
add_custom_target(compile-ui-glow-material DEPENDS "${UI_GLOW_MATERIAL_ARTIFACT}")

add_material_ir("world soft-target" world-soft-target-material
  "Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/SoftTargetWorld.lispb"
  SoftTargetWorld WORLD_SOFT_TARGET_MATERIAL_ARTIFACT)
add_custom_target(compile-world-soft-target-material
  DEPENDS "${WORLD_SOFT_TARGET_MATERIAL_ARTIFACT}"
)

set(MIGRATED_MATERIAL_ARTIFACTS)
set(SHADER_MATERIAL_SOURCE_ROOT
  "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials"
)
add_material_ir("radar display" radar-display-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/RadarDisplay.lispb" RadarDisplay MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("energy shield" energy-shield-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/EnergyShield.lispb" EnergyShield MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("space energy field" space-energy-field-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/SpaceEnergyFieldDisplay.lispb" SpaceEnergyField MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("glowing cross" glowing-cross-material
  "Source/Sandbox/materials/GlowingCross.lispb" GlowingCross MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("vertex ripple" vertex-ripple-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/VertexRipple.lispb" VertexRipple MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("construction spawn" construction-spawn-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/ConstructionSpawn.lispb" ConstructionSpawn MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("planet atmosphere" planet-atmosphere-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/PlanetAtmosphere.lispb" PlanetAtmosphere MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("planet surface" planet-surface-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/PlanetSurface.lispb" PlanetSurface MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("tactical scan" tactical-scan-material
  "${SHADER_MATERIAL_SOURCE_ROOT}/TacticalScan.lispb" TacticalScan MIGRATED_MATERIAL_ARTIFACTS)
add_material_ir("celestial analytic" celestial-analytic-material
  "Plugins/SandboxShaders/Source/SandboxCelestials/Private/materials/CelestialAnalytic.lispb"
  CelestialAnalytic MIGRATED_MATERIAL_ARTIFACTS)
add_custom_target(compile-migrated-materials DEPENDS ${MIGRATED_MATERIAL_ARTIFACTS})

add_custom_target(validate-ui-glow-material
  COMMAND $<TARGET_FILE:lispb> validate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target ui-glow-material
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(validate-world-soft-target-material
  COMMAND $<TARGET_FILE:lispb> validate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target world-soft-target-material
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)

# Unreal commandlets consume these generated artifacts.
set(UI_GLOW_MATERIAL_ARTIFACT "${UI_GLOW_MATERIAL_ARTIFACT}" PARENT_SCOPE)
set(WORLD_SOFT_TARGET_MATERIAL_ARTIFACT "${WORLD_SOFT_TARGET_MATERIAL_ARTIFACT}" PARENT_SCOPE)

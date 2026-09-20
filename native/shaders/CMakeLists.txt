find_program(SANDBOX_DXC_EXECUTABLE
  NAMES dxc dxc.exe
  REQUIRED
  DOC "Path to the DirectX Shader Compiler"
)

function(sandbox_add_dxc_shader_test)
  cmake_parse_arguments(PARSE_ARGV 0 shader "" "NAME;SOURCE;ENTRY_POINT;PROFILE" "")

  if(shader_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "sandbox_add_dxc_shader_test received unexpected arguments: ${shader_UNPARSED_ARGUMENTS}"
    )
  endif()

  foreach(required_argument IN ITEMS NAME SOURCE PROFILE)
    if(NOT shader_${required_argument})
      message(FATAL_ERROR
        "sandbox_add_dxc_shader_test requires ${required_argument}."
      )
    endif()
  endforeach()

  if(NOT EXISTS "${shader_SOURCE}")
    message(FATAL_ERROR "DXC shader source does not exist: ${shader_SOURCE}")
  endif()

  set(dxc_command
    "${SANDBOX_DXC_EXECUTABLE}"
    -T "${shader_PROFILE}"
  )
  if(shader_ENTRY_POINT)
    list(APPEND dxc_command -E "${shader_ENTRY_POINT}")
  endif()
  list(APPEND dxc_command
    -DSANDBOX_STANDALONE_DXC=1
    -WX
    -Fo "${CMAKE_CURRENT_BINARY_DIR}/${shader_NAME}.dxil"
    "${shader_SOURCE}"
  )

  add_test(NAME "${shader_NAME}" COMMAND ${dxc_command})
  set_tests_properties("${shader_NAME}" PROPERTIES
    LABELS "unit;all;native;native-shader"
  )
endfunction()

set(sandbox_ui_shader_root "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Shaders/Private")
set(sandbox_shaders_root "${PROJECT_SOURCE_DIR}/Plugins/SandboxShaders/Shaders")
set(space_game_shader_root "${PROJECT_SOURCE_DIR}/Plugins/SpaceGame/Shaders/Private")

set(heatmap_rdg_shader "${sandbox_ui_shader_root}/HeatmapRDG/HeatmapRDG.usf")
sandbox_add_dxc_shader_test(
  NAME heatmap-rdg-dxc-test
  SOURCE "${heatmap_rdg_shader}"
  ENTRY_POINT render_heatmap_cs
  PROFILE cs_6_0
)

set(radar_shader "${sandbox_ui_shader_root}/Radar/Radar.usf")
sandbox_add_dxc_shader_test(
  NAME radar-plane-vs-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_plane_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-plane-ps-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_plane_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-structure-vs-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_structure_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-stem-vs-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_stem_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-line-ps-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_line_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-glyph-vs-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_glyph_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME radar-glyph-ps-dxc-test
  SOURCE "${radar_shader}"
  ENTRY_POINT radar_glyph_ps
  PROFILE ps_6_0
)

set(scatter_3d_shader "${sandbox_ui_shader_root}/Scatter3D/Scatter3D.usf")
sandbox_add_dxc_shader_test(
  NAME scatter-3d-background-vs-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_background_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME scatter-3d-background-ps-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_background_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME scatter-3d-frame-vs-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_frame_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME scatter-3d-frame-ps-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_frame_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME scatter-3d-sample-vs-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_sample_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME scatter-3d-sample-ps-dxc-test
  SOURCE "${scatter_3d_shader}"
  ENTRY_POINT scatter_sample_ps
  PROFILE ps_6_0
)

set(volume_heatmap_3d_shader
  "${sandbox_ui_shader_root}/VolumeHeatmap3D/VolumeHeatmap3D.usf"
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-background-vs-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_background_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-background-ps-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_background_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-slice-vs-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_slice_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-slice-ps-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_slice_ps
  PROFILE ps_6_0
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-frame-vs-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_frame_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME volume-heatmap-3d-frame-ps-dxc-test
  SOURCE "${volume_heatmap_3d_shader}"
  ENTRY_POINT volume_frame_ps
  PROFILE ps_6_0
)

set(entity_overlay_shader "${sandbox_ui_shader_root}/EntityOverlay/EntityOverlay.usf")
sandbox_add_dxc_shader_test(
  NAME entity-overlay-vs-dxc-test
  SOURCE "${entity_overlay_shader}"
  ENTRY_POINT entity_overlay_vs
  PROFILE vs_6_0
)
sandbox_add_dxc_shader_test(
  NAME entity-overlay-ps-dxc-test
  SOURCE "${entity_overlay_shader}"
  ENTRY_POINT entity_overlay_ps
  PROFILE ps_6_0
)

set(nebula_density_shader
  "${sandbox_shaders_root}/Private/Nebula/NebulaDensity.usf"
)
sandbox_add_dxc_shader_test(
  NAME nebula-density-dxc-test
  SOURCE "${nebula_density_shader}"
  ENTRY_POINT generate_nebula_density_cs
  PROFILE cs_6_0
)

set(space_energy_field_shader
  "${sandbox_shaders_root}/Private/SpaceEnergyField/SpaceEnergyField.usf"
)
sandbox_add_dxc_shader_test(
  NAME space-energy-field-dxc-test
  SOURCE "${space_energy_field_shader}"
  ENTRY_POINT render_space_energy_field_cs
  PROFILE cs_6_0
)

set(spark_expansion_shader "${space_game_shader_root}/Sparks/SparkExpansion.usf")
sandbox_add_dxc_shader_test(
  NAME spark-expansion-dxc-test
  SOURCE "${spark_expansion_shader}"
  ENTRY_POINT expand_spark_bursts_cs
  PROFILE cs_6_0
)

set(sandbox_shader_libraries
  Common/ProceduralNoise.ush
  Private/CelestialBackdrop/CelestialBackdrop.ush
  Private/ConstructionSpawn/ConstructionSpawn.ush
  Private/EnergyBeam/EnergyBeam.ush
  Private/EnergyShield/EnergyShield.ush
  Private/EngineExhaust/EngineExhaust.ush
  Private/Nebula/NebulaBackdrop.ush
  Private/Nebula/NebulaCommon.ush
  Private/Nebula/NebulaVolume.ush
  Private/PlanetAtmosphere/PlanetAtmosphere.ush
  Private/RadarDisplay/RadarDisplay.ush
  Private/RaymarchedAnomaly/RaymarchedAnomaly.ush
  Private/ShieldImpact/ShieldImpact.ush
  Private/TacticalScan/TacticalScan.ush
  Private/VertexRipple/VertexRipple.ush
  Private/WarpField/WarpField.ush
)

foreach(shader_library IN LISTS sandbox_shader_libraries)
  get_filename_component(shader_library_name "${shader_library}" NAME_WE)
  string(TOLOWER "${shader_library_name}" shader_library_name)
  string(REPLACE "_" "-" shader_library_name "${shader_library_name}")
  sandbox_add_dxc_shader_test(
    NAME "${shader_library_name}-library-dxc-test"
    SOURCE "${sandbox_shaders_root}/${shader_library}"
    PROFILE lib_6_3
  )
endforeach()

find_program(SANDBOX_DXC_EXECUTABLE NAMES dxc dxc.exe)

set(heatmap_rdg_shader
  "${PROJECT_SOURCE_DIR}/Plugins/SandboxUI/Shaders/Private/HeatmapRDG/HeatmapRDG.usf"
)
set(heatmap_rdg_dxil "${CMAKE_CURRENT_BINARY_DIR}/HeatmapRDG.dxil")

add_test(
  NAME heatmap-rdg-dxc-test
  COMMAND "${CMAKE_COMMAND}"
    "-DDXC_EXECUTABLE=${SANDBOX_DXC_EXECUTABLE}"
    "-DSHADER_SOURCE=${heatmap_rdg_shader}"
    "-DSHADER_OUTPUT=${heatmap_rdg_dxil}"
    -P "${CMAKE_CURRENT_LIST_DIR}/compile_heatmap_rdg.cmake"
)
set_tests_properties(heatmap-rdg-dxc-test PROPERTIES
  LABELS "unit;all;native;native-shader"
)

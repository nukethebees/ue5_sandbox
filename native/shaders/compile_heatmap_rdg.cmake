if(NOT DXC_EXECUTABLE OR NOT EXISTS "${DXC_EXECUTABLE}")
  message(FATAL_ERROR
    "DXC missing: install dxc.exe and make it available on PATH before configuring the native tests.")
endif()

execute_process(
  COMMAND "${DXC_EXECUTABLE}"
    -T cs_6_0
    -E render_heatmap_cs
    -DSANDBOX_STANDALONE_DXC=1
    -WX
    -Fo "${SHADER_OUTPUT}"
    "${SHADER_SOURCE}"
  RESULT_VARIABLE dxc_result
  OUTPUT_VARIABLE dxc_output
  ERROR_VARIABLE dxc_errors
)

if(NOT dxc_result EQUAL 0)
  message(FATAL_ERROR
    "DXC failed to compile HeatmapRDG.usf (exit ${dxc_result}).\n"
    "Compiler output:\n${dxc_output}${dxc_errors}")
endif()

message(STATUS "DXC compiled HeatmapRDG.usf successfully.\n${dxc_output}${dxc_errors}")

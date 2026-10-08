add_library(native-ui STATIC)
target_sources(native-ui
  PRIVATE
    src/chart_layout.cpp
    src/entity_overlay.cpp
    src/graph_plot.cpp
    src/heatmap_2d.cpp
    src/histogram.cpp
    src/radar_2d.cpp
    src/settings_slider.cpp
    src/stacked_bar_chart.cpp
)
target_include_directories(native-ui
  PUBLIC
    include
)
target_link_libraries(native-ui
  PUBLIC
    sandbox::native_defaults
)
ioj_enable_warnings(native-ui)
ioj_apply_native_abi(native-ui)
if(IOJ_WITH_UNREAL)
  set(ui_output_dir "${IOJ_NATIVE_OUTPUT_ROOT}/UI/${IOJ_NATIVE_TOOLCHAIN}/${UE_PLATFORM}")
  set_target_properties(native-ui PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${ui_output_dir}/${UE_CONFIGURATION}"
  )
endif()

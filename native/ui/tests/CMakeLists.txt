add_executable(native-ui-tests)
target_sources(native-ui-tests
  PRIVATE
    graph_plot_tests.cpp
    ui_entity_overlay_tests.cpp
    ui_heatmap_2d_tests.cpp
    ui_radar_stacked_bar_tests.cpp
    ui_settings_histogram_tests.cpp
)
target_link_libraries(native-ui-tests
  PRIVATE
    native-ui
    GTest::gtest_main
    ioj::test_pch
)
ioj_enable_warnings(native-ui-tests)
ioj_apply_native_abi(native-ui-tests)
add_test(NAME native-ui-tests COMMAND native-ui-tests)
set_tests_properties(native-ui-tests PROPERTIES LABELS "unit;all;native;native-ui")

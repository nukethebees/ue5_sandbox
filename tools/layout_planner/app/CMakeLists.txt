add_executable(layout-planner)
target_sources(layout-planner PRIVATE
  application.cpp
  platform/file_dialog.cpp
  gui/planner_ui.cpp
  gui/planner_ui_browser.cpp
  gui/planner_ui_browser_representations.cpp
  gui/planner_ui_browser_aggregates.cpp
  gui/planner_ui_comparison.cpp
  gui/planner_ui_comparison_targets.cpp
  gui/planner_ui_graph.cpp
  gui/planner_ui_properties.cpp
  gui/planner_ui_properties_scalar.cpp
  gui/planner_ui_properties_packed.cpp
  gui/planner_ui_properties_aggregate.cpp
  gui/planner_ui_properties_soa.cpp
  gui/planner_ui_properties_structural.cpp
  gui/planner_ui_support.cpp
  gui/planner_ui_visualizations.cpp
  main.cpp
)
target_compile_features(layout-planner PRIVATE cxx_std_23)
target_include_directories(layout-planner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(layout-planner PRIVATE
  CLI11::CLI11
  SDL3::SDL3-static
  nfd::nfd
  native-layout
  lispb-target-compiler
  sandbox-imgui
  sandbox::warnings_as_errors
)
if(WIN32)
  target_link_libraries(layout-planner PRIVATE ole32 uuid shell32)
endif()
target_compile_definitions(layout-planner PRIVATE SDL_MAIN_HANDLED)

add_executable(layout-planner-ui-tests tests/table_tests.cpp gui/planner_ui_support.cpp)
target_compile_features(layout-planner-ui-tests PRIVATE cxx_std_23)
target_include_directories(layout-planner-ui-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(layout-planner-ui-tests PRIVATE
  sandbox-imgui native-layout GTest::gtest_main sandbox::warnings_as_errors)
add_test(NAME layout-planner-ui-tests COMMAND layout-planner-ui-tests)
set_tests_properties(layout-planner-ui-tests PROPERTIES LABELS "unit;native;layout;tools")

install(TARGETS layout-planner
  RUNTIME DESTINATION bin
  COMPONENT layout-planner
)

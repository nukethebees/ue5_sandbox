add_executable(layout-planner)
target_sources(layout-planner PRIVATE
  application.cpp
  gui/planner_ui.cpp
  gui/planner_ui_browser.cpp
  gui/planner_ui_comparison.cpp
  gui/planner_ui_graph.cpp
  gui/planner_ui_properties.cpp
  gui/planner_ui_support.cpp
  gui/planner_ui_visualizations.cpp
  main.cpp
)
target_compile_features(layout-planner PRIVATE cxx_std_23)
target_include_directories(layout-planner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(layout-planner PRIVATE
  CLI11::CLI11
  SDL3::SDL3-static
  native-layout
  sandbox-imgui
  sandbox::warnings_as_errors
)
target_compile_definitions(layout-planner PRIVATE SDL_MAIN_HANDLED)

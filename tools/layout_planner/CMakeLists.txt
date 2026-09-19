add_executable(layout-planner)
target_sources(layout-planner PRIVATE
  src/application.cpp
  src/main.cpp
  src/planner_ui.cpp
)
target_compile_features(layout-planner PRIVATE cxx_std_23)
target_include_directories(layout-planner PRIVATE src)
target_link_libraries(layout-planner PRIVATE
  CLI11::CLI11
  SDL3::SDL3-static
  native-layout
  sandbox-imgui
  sandbox::warnings_as_errors
)
target_compile_definitions(layout-planner PRIVATE SDL_MAIN_HANDLED)

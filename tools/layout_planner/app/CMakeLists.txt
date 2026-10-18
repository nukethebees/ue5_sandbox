add_library(layout-planner-ui)
target_sources(layout-planner-ui
  PRIVATE
    platform/file_dialog.cpp
    gui/planner_ui.cpp
)
target_include_directories(layout-planner-ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(layout-planner-ui PUBLIC
  CLI11::CLI11
  SDL3::SDL3-static
  nfd::nfd
  native-layout
  sandbox-imgui
  sandbox::native_defaults
  "$<$<BOOL:${WIN32}>:ole32;uuid;shell32>"
)
ioj_enable_warnings(layout-planner-ui CODEGEN)
add_executable(layout-planner application.cpp main.cpp)
ioj_enable_warnings(layout-planner CODEGEN)
target_link_libraries(layout-planner PRIVATE layout-planner-ui)
target_compile_definitions(layout-planner PRIVATE SDL_MAIN_HANDLED)
target_compile_definitions(layout-planner-ui PRIVATE SDL_MAIN_HANDLED)

add_executable(layout-planner-ui-tests)
target_sources(layout-planner-ui-tests
  PRIVATE
    tests/planner_ui_tests.cpp
)
target_link_libraries(layout-planner-ui-tests PRIVATE
  layout-planner-ui GTest::gtest_main)
ioj_enable_warnings(layout-planner-ui-tests)
target_compile_definitions(layout-planner-ui-tests PRIVATE IOJ_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME layout-planner-ui-tests COMMAND layout-planner-ui-tests)
set_tests_properties(layout-planner-ui-tests PROPERTIES LABELS "unit;layout;developer-tool")

install(TARGETS layout-planner
  RUNTIME DESTINATION bin
  COMPONENT layout-planner
)

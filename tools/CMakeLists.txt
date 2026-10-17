if(NOT IOJ_BUILD_DEVELOPER_TOOLS)
  return()
endif()

add_subdirectory(rust)

add_subdirectory(layout_planner)
add_subdirectory(image_lab)

add_custom_target(developer-tools-build
  DEPENDS rust-tool-tests-build native-layout-tests layout-planner-ui-tests image-lab-tests
)
add_custom_target(task-start-build
  DEPENDS native-tests developer-tools-build
)

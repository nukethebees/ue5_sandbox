add_executable(image-lab
  application.cpp
  main.cpp
)
target_compile_features(image-lab PRIVATE cxx_std_23)
target_include_directories(image-lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(image-lab PRIVATE
  SDL3::SDL3-static
  image-lab-workflow
  sandbox-image
  sandbox-imgui-sdlrenderer
  sandbox::native_defaults
)
target_compile_definitions(image-lab PRIVATE
  SDL_MAIN_HANDLED
  SANDBOX_IMAGE_LAB_PROJECT_DIRECTORY="${PROJECT_SOURCE_DIR}"
)

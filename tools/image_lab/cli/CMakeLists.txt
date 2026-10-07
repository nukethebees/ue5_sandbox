add_executable(image-lab-cli main.cpp)
target_link_libraries(image-lab-cli PRIVATE
  CLI11::CLI11
  image-lab-workflow
  sandbox::native_defaults
)
ioj_enable_warnings(image-lab-cli)

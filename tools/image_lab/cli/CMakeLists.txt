add_executable(image-lab-cli main.cpp)
target_compile_features(image-lab-cli PRIVATE cxx_std_23)
target_link_libraries(image-lab-cli PRIVATE
  CLI11::CLI11
  image-lab-workflow
  sandbox::native_defaults
)

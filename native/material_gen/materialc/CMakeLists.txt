add_executable(materialc)
target_sources(materialc PRIVATE main.cpp)
target_compile_features(materialc PRIVATE cxx_std_23)
target_link_libraries(materialc PRIVATE
  material-gen
  sandbox::codegen_warnings
)

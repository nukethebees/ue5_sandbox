add_executable(mesh-gen-tests mesh_generation_tests.cpp)
target_compile_features(mesh-gen-tests PRIVATE cxx_std_23)
target_link_libraries(mesh-gen-tests PRIVATE
  mesh-gen
  GTest::gtest_main
  sandbox::warnings_as_errors
)

include(GoogleTest)
gtest_discover_tests(mesh-gen-tests PROPERTIES LABELS "unit;all;native-mesh")

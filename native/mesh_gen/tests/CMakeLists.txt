add_executable(mesh-gen-tests mesh_generation_tests.cpp)
ioj_stage_runtime_dependencies(mesh-gen-tests)
target_link_libraries(mesh-gen-tests PRIVATE
  mesh-gen
  GTest::gtest_main
  native-test-support
  sandbox::native_defaults
)
ioj_enable_warnings(mesh-gen-tests)

add_test(NAME mesh-gen-tests COMMAND mesh-gen-tests)
set_tests_properties(mesh-gen-tests PROPERTIES LABELS "unit;all;native;native-mesh")

add_executable(native-layout-tests)
add_dependencies(native-layout-tests generate-native-soa-fixture)
target_sources(native-layout-tests PRIVATE
  analyzer_tests.cpp
  analyzer_aggregate_tests.cpp
  analyzer_packed_tests.cpp
  analyzer_soa_tests.cpp
  analyzer_common_tests.cpp
  physical_facts_tests.cpp
  frame_pacer_tests.cpp
  planner_type_tests.cpp
  planner_selection_tests.cpp
  planner_session_tests.cpp
  schema_loader_tests.cpp
  workspace_tests.cpp
)
target_compile_features(native-layout-tests PRIVATE cxx_std_23)
target_link_libraries(native-layout-tests PRIVATE
  native-layout
  native-soa
  GTest::gtest_main
  sandbox::native_defaults
  ioj::test_pch
)
target_compile_definitions(native-layout-tests PRIVATE
  IOJ_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME native-layout-tests COMMAND native-layout-tests)
set_tests_properties(native-layout-tests PROPERTIES LABELS "unit;all;native;layout")

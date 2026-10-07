add_library(image-lab-workflow STATIC)
target_sources(image-lab-workflow PRIVATE
  src/workflow.cpp
)
target_include_directories(image-lab-workflow PUBLIC include)
target_link_libraries(image-lab-workflow
  PUBLIC
    sandbox::native_defaults
    sandbox-image
  PRIVATE
    nlohmann_json::nlohmann_json
)
ioj_enable_warnings(image-lab-workflow)

add_executable(image-lab-tests
  tests/workflow_tests.cpp
)
target_link_libraries(image-lab-tests PRIVATE
  image-lab-workflow
  GTest::gtest_main
  sandbox::native_defaults
)
ioj_enable_warnings(image-lab-tests)
add_test(NAME image-lab-tests COMMAND image-lab-tests)
set_tests_properties(image-lab-tests PROPERTIES LABELS "unit;integration;image-lab;developer-tool")

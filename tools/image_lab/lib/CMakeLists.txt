add_library(image-lab-workflow STATIC)
target_sources(image-lab-workflow PRIVATE
  src/workflow.cpp
)
target_compile_features(image-lab-workflow PUBLIC cxx_std_23)
target_include_directories(image-lab-workflow PUBLIC include)
target_link_libraries(image-lab-workflow PUBLIC sandbox-image PRIVATE sandbox::native_defaults)

add_executable(image-lab-tests
  tests/workflow_tests.cpp
)
target_compile_features(image-lab-tests PRIVATE cxx_std_23)
target_link_libraries(image-lab-tests PRIVATE
  image-lab-workflow
  GTest::gtest_main
  sandbox::native_defaults
)
add_test(NAME image-lab-tests COMMAND image-lab-tests)
set_tests_properties(image-lab-tests PROPERTIES LABELS "unit;image-lab")

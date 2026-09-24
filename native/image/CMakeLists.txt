set(IOJ_IMAGE_SOURCES
  src/image_generation.cpp
  src/image_generation_utilities.cpp
  src/image_preview.cpp
  src/noise_sampling.cpp
)

add_library(sandbox-image STATIC ${IOJ_IMAGE_SOURCES})
target_compile_features(sandbox-image PUBLIC cxx_std_23)
target_include_directories(sandbox-image
  PUBLIC include
  PRIVATE src
)
target_link_libraries(sandbox-image PRIVATE sandbox::native_defaults ioj::core_pch)
set_target_properties(sandbox-image PROPERTIES
  OUTPUT_NAME sandbox_image
)

add_executable(sandbox-image-tests
  ${IOJ_IMAGE_SOURCES}
  tests/image_generation_tests.cpp
)
target_compile_features(sandbox-image-tests PRIVATE cxx_std_23)
target_include_directories(sandbox-image-tests PRIVATE include src tests)
target_link_libraries(sandbox-image-tests PRIVATE
  sandbox::native_defaults
  GTest::gtest_main
  ioj::test_pch
)

add_test(NAME sandbox-image-tests COMMAND sandbox-image-tests)
set_tests_properties(sandbox-image-tests PROPERTIES LABELS "unit;all;native;native-image")

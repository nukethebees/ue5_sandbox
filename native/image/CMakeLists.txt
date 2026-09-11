find_package(GTest CONFIG REQUIRED)

foreach(configuration IN ITEMS Debug DebugGame Development Shipping Test)
  file(MAKE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/Image/${UE_PLATFORM}/${configuration}"
  )
endforeach()

set(SANDBOX_IMAGE_SOURCES
  src/image_generation.cpp
  src/image_generation_utilities.cpp
  src/image_preview.cpp
  src/noise_sampling.cpp
)

add_library(sandbox-image STATIC ${SANDBOX_IMAGE_SOURCES})
target_compile_features(sandbox-image PUBLIC cxx_std_23)
target_include_directories(sandbox-image
  PUBLIC include
  PRIVATE src
)
target_link_libraries(sandbox-image PRIVATE sandbox::warnings_as_errors)
set_target_properties(sandbox-image PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY
    "${PROJECT_SOURCE_DIR}/Binaries/Native/Image/${UE_PLATFORM}/${UE_CONFIGURATION}"
  OUTPUT_NAME sandbox_image
)

if(WIN32)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
     NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(FATAL_ERROR "sandbox-image requires an MSVC-compatible ABI on Windows.")
  endif()

  set_target_properties(sandbox-image PROPERTIES
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL
  )
  target_compile_definitions(sandbox-image PRIVATE NDEBUG _ITERATOR_DEBUG_LEVEL=0)
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(sandbox-image PRIVATE /U_DEBUG)
  else()
    target_compile_options(sandbox-image PRIVATE -U_DEBUG)
  endif()
endif()

add_executable(sandbox-image-tests
  ${SANDBOX_IMAGE_SOURCES}
  tests/image_generation_tests.cpp
)
target_compile_features(sandbox-image-tests PRIVATE cxx_std_23)
target_include_directories(sandbox-image-tests PRIVATE include src tests)
target_link_libraries(sandbox-image-tests PRIVATE
  sandbox::warnings_as_errors
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(sandbox-image-tests PROPERTIES LABELS "unit;all;native-image")

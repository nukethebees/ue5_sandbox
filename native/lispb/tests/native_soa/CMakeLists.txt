add_custom_target(check-generated-native-soa-fixture
  COMMAND "$<TARGET_FILE:lispb-executable>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
  DEPENDS lispb-executable
  VERBATIM
)
add_dependencies(check-generated-code check-generated-native-soa-fixture)

add_executable(native-soa-tests)
target_sources(native-soa-tests
  PRIVATE
    tests.cpp
    src/types.cpp
    include/native_soa/types.hpp
)
ioj_enable_warnings(native-soa-tests)
target_include_directories(native-soa-tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/native/core/include"
)
target_link_libraries(native-soa-tests
  PRIVATE
    sandbox::native_defaults
    GTest::gtest_main
)
add_test(NAME native-soa-tests COMMAND native-soa-tests)
set_tests_properties(native-soa-tests PROPERTIES LABELS "unit;all;native;native-soa")

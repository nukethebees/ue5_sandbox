add_library(native-layout)
target_sources(native-layout PRIVATE
  src/abi_profile.cpp
  src/analyzer.cpp
  src/frame_pacer.cpp
  src/lispb_adapter.cpp
  src/model.cpp
  src/workspace.cpp
)
target_compile_features(native-layout PUBLIC cxx_std_23)
target_include_directories(native-layout PUBLIC include)
target_link_libraries(native-layout
  PRIVATE
    codegen-lib
    lispb-project
    sandbox::warnings_as_errors
)

add_executable(native-layout-tests)
target_sources(native-layout-tests PRIVATE
  tests/analyzer_tests.cpp
  tests/frame_pacer_tests.cpp
  tests/lispb_adapter_tests.cpp
  tests/workspace_tests.cpp
)
target_compile_features(native-layout-tests PRIVATE cxx_std_23)
target_link_libraries(native-layout-tests PRIVATE
  native-layout
  GTest::gtest_main
  sandbox::warnings_as_errors
)
target_compile_definitions(native-layout-tests PRIVATE
  SANDBOX_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME native-layout-tests COMMAND native-layout-tests)
set_tests_properties(native-layout-tests PROPERTIES LABELS "unit;all;native;layout")

add_executable(native-level-authoring-tests)
target_sources(native-level-authoring-tests PRIVATE level_authoring_tests.cpp)
target_compile_features(native-level-authoring-tests PRIVATE cxx_std_23)
target_link_libraries(native-level-authoring-tests PRIVATE
  native-level-authoring
  GTest::gtest_main
  sandbox::warnings_as_errors
)
if(SANDBOX_WITH_TRACY AND SANDBOX_WITH_UNREAL)
  target_link_libraries(native-level-authoring-tests PRIVATE Tracy::TracyClient)
  add_custom_command(TARGET native-level-authoring-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:TracyClient>"
      "$<TARGET_FILE_DIR:native-level-authoring-tests>"
    VERBATIM
  )
endif()
configure_sandbox_s7_abi(native-level-authoring-tests)

include(GoogleTest)
gtest_discover_tests(native-level-authoring-tests PROPERTIES
  LABELS "unit;all;native-level-authoring")

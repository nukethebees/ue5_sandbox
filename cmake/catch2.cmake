function(add_low_level_test_suite target_name unreal_target test_name_prefix test_label)
  add_unreal_target(${target_name} ${unreal_target})

  set(test_executable
    "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/${UE_PLATFORM}/${unreal_target}/${unreal_target}-${UE_PLATFORM}-${UE_CONFIGURATION}.exe"
  )

  discover_tests(
    COMMAND "${test_executable}"
    DISCOVERY_ARGS --list-tests --reporter JSON
    DISCOVERY_MATCH "^[ \\t]*\\\"name\\\": \\\"(${test_name_prefix}\\.[^\\\"]+)\\\",$"
    TEST_NAME "\\1"
    TEST_ARGS "\\1"
    TEST_PROPERTIES
    LABELS ${test_label}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endfunction()

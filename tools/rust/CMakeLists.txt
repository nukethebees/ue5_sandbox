add_custom_target(rust-tool-tests-build
  COMMAND "${IOJ_CARGO_EXECUTABLE}" test --locked --no-run
    --package coj --package jobserver-client --package native-binary-tools --package game-package-tools --package benchmark-tools
    --target-dir "${SANDBOX_RUST_TARGET_DIRECTORY}"
  WORKING_DIRECTORY "${SANDBOX_RUST_DIRECTORY}"
  VERBATIM
)

foreach(package IN ITEMS coj jobserver-client native-binary-tools game-package-tools benchmark-tools)
  add_test(NAME ${package}
    COMMAND "${IOJ_CARGO_EXECUTABLE}" test --locked --package "${package}"
      --target-dir "${SANDBOX_RUST_TARGET_DIRECTORY}"
  )
  set_tests_properties(${package} PROPERTIES
    WORKING_DIRECTORY "${SANDBOX_RUST_DIRECTORY}"
    LABELS "developer-tool;rust;${package};unit"
    RESOURCE_LOCK rust-tools
  )
endforeach()

if(TARGET jobserver-test-server)
  add_dependencies(rust-tool-tests-build jobserver-test-server)
  add_test(NAME jobserver-client-integration
    COMMAND "${IOJ_CARGO_EXECUTABLE}" test --locked --package jobserver-client
      --test server -- --ignored
  )
  set_tests_properties(jobserver-client-integration PROPERTIES
    WORKING_DIRECTORY "${SANDBOX_RUST_DIRECTORY}"
    ENVIRONMENT "IOJ_JOBSERVER_TEST_DAEMON=$<TARGET_FILE:jobserver-test-server>;CARGO_TARGET_DIR=${SANDBOX_RUST_TARGET_DIRECTORY}"
    LABELS "developer-tool;rust;jobserver;integration"
    RESOURCE_LOCK rust-tools
    TIMEOUT 120
  )
endif()

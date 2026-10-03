add_custom_target(rust-tool-tests-build
  COMMAND "${IOJ_CARGO_EXECUTABLE}" test --locked --no-run
    --package agent-task --package native-binary-tools --package game-package-tools --package benchmark-tools
    --target-dir "${SANDBOX_RUST_TARGET_DIRECTORY}"
  WORKING_DIRECTORY "${SANDBOX_RUST_DIRECTORY}"
  VERBATIM
)

foreach(package IN ITEMS agent-task native-binary-tools game-package-tools benchmark-tools)
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

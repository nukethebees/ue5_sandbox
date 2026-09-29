include_guard(GLOBAL)
find_program(IOJ_CARGO_EXECUTABLE cargo REQUIRED)
set(SANDBOX_RUST_DIRECTORY "${PROJECT_SOURCE_DIR}/tools/rust")
set(SANDBOX_RUST_TARGET_DIRECTORY "${CMAKE_BINARY_DIR}/rust-tools")

function(sandbox_add_rust_host_tool package output_variable)
  set(executable "${SANDBOX_RUST_TARGET_DIRECTORY}/release/${package}${CMAKE_EXECUTABLE_SUFFIX}")
  add_custom_target(${package}-host
    COMMAND "${IOJ_CARGO_EXECUTABLE}" build --locked --release --package "${package}"
      --target-dir "${SANDBOX_RUST_TARGET_DIRECTORY}"
    WORKING_DIRECTORY "${SANDBOX_RUST_DIRECTORY}"
    BYPRODUCTS "${executable}"
    COMMENT "Building Rust host tool ${package}"
    VERBATIM
  )
  add_executable(${package}-executable IMPORTED GLOBAL)
  set_target_properties(${package}-executable PROPERTIES IMPORTED_LOCATION "${executable}")
  add_dependencies(${package}-executable ${package}-host)
  set(${output_variable} "$<TARGET_FILE:${package}-executable>" PARENT_SCOPE)
endfunction()

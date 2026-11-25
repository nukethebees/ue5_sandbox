set(host_directory "${PROJECT_SOURCE_DIR}/native/sandbox_host")
add_executable(embed-host-identity)
target_sources(embed-host-identity PRIVATE
  "${host_directory}/embed_build_identity.cpp"
  "${host_directory}/build_identity.cpp"
)
target_link_libraries(embed-host-identity PRIVATE native-core sandbox::native_defaults)

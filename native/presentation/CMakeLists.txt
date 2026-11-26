add_library(native-presentation-interface INTERFACE)
target_include_directories(native-presentation-interface
  INTERFACE
    include
    ${PROJECT_SOURCE_DIR}/native/levels/lib/include
    ${PROJECT_SOURCE_DIR}/native/profiles/lib/include
)
target_link_libraries(native-presentation-interface
  INTERFACE
    native-core-headers
    sandbox::native_defaults
)

add_subdirectory(bridge)
add_subdirectory(lib)
add_subdirectory(tests)

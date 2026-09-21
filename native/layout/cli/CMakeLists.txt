add_executable(layout-profile-probe)
target_sources(layout-profile-probe PRIVATE
  main.cpp
)
target_compile_features(layout-profile-probe PRIVATE cxx_std_23)
target_link_libraries(layout-profile-probe PRIVATE
  native-layout
  sandbox::warnings_as_errors
)

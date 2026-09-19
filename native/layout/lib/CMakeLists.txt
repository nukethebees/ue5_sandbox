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

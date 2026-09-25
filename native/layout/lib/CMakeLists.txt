add_library(native-layout)
target_sources(native-layout PRIVATE
  src/abi_profile.cpp
  src/physical_facts.cpp
  src/analyzer.cpp
  src/analyzer_scalar.cpp
  src/analyzer_aggregate.cpp
  src/analyzer_record_access.cpp
  src/analyzer_packed.cpp
  src/analyzer_soa.cpp
  src/frame_pacer.cpp
  src/planner_selection.cpp
  src/planner_session.cpp
  src/planner_session_analysis.cpp
  src/planner_type.cpp
  src/schema_loader.cpp
  src/workspace.cpp
)
target_compile_features(native-layout PUBLIC cxx_std_23)
target_include_directories(native-layout PUBLIC include)
target_compile_definitions(native-layout PRIVATE
  IOJ_LAYOUT_TARGET_PLATFORM="${CMAKE_SYSTEM_NAME}"
  IOJ_LAYOUT_TARGET_ARCHITECTURE="${CMAKE_SYSTEM_PROCESSOR}"
  IOJ_LAYOUT_TARGET_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
  IOJ_LAYOUT_BUILD_CONFIGURATION="$<CONFIG>"
)
target_link_libraries(native-layout
  PUBLIC
    lispb-schema
  PRIVATE
    lispb-project
    sandbox::native_defaults
)

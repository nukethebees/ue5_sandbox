add_library(native-layout)
set_target_properties(native-layout PROPERTIES UNITY_BUILD OFF)
target_sources(native-layout PRIVATE
  src/document.cpp
  src/document_io.cpp
  src/document_file_updates.cpp
  src/document_source_edit.cpp
  src/document_state.cpp
  src/transaction.cpp
  src/abi_profile.cpp
  src/profile_probe.cpp
  src/graph_view.cpp
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
  src/variants.cpp
  src/planner_type.cpp
  src/workspace.cpp
)
target_include_directories(native-layout PUBLIC include)
target_compile_definitions(native-layout PRIVATE
  IOJ_LAYOUT_TARGET_PLATFORM="${CMAKE_SYSTEM_NAME}"
  IOJ_LAYOUT_TARGET_ARCHITECTURE="${CMAKE_SYSTEM_PROCESSOR}"
  IOJ_LAYOUT_TARGET_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
  IOJ_LAYOUT_BUILD_CONFIGURATION="$<CONFIG>"
)
target_link_libraries(native-layout
  PUBLIC
    sandbox::native_defaults
    lispb-schema
  PRIVATE
    lispb-project
)
ioj_enable_warnings(native-layout)

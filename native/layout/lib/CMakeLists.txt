set(layout_analysis_generated "${CMAKE_BINARY_DIR}/native/layout/generated")
set(layout_analysis_tables
  code_table
  component_table
  distribution_entry_table
  distribution_table
  packed_field_table
  relationship_table
  scalar_table
  selection_table
  soa_column_table
  tag_table
  type_analysis_table
  workload_table
)
set(layout_analysis_headers)
set(layout_analysis_sources)
set(layout_analysis_schemas "${CMAKE_CURRENT_SOURCE_DIR}/../schema/types.lispb")
foreach(table IN LISTS layout_analysis_tables)
  list(APPEND layout_analysis_headers "${layout_analysis_generated}/ioj/layout/analysis/${table}.hpp")
  list(APPEND layout_analysis_sources "${layout_analysis_generated}/ioj/layout/analysis/${table}.cpp")
  list(APPEND layout_analysis_schemas "${CMAKE_CURRENT_SOURCE_DIR}/../schema/${table}.lispb")
endforeach()

add_custom_command(
  OUTPUT ${layout_analysis_headers} ${layout_analysis_sources}
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target layout-analysis
    --depfile "${layout_analysis_generated}/analysis.d"
  DEPENDS lispb "${PROJECT_SOURCE_DIR}/lispb/project.lispb" ${layout_analysis_schemas}
  DEPFILE "${layout_analysis_generated}/analysis.d"
  VERBATIM
)
add_custom_target(generate-layout-analysis DEPENDS ${layout_analysis_headers} ${layout_analysis_sources})

add_library(native-layout)
add_dependencies(native-layout generate-layout-analysis)
set_target_properties(native-layout PROPERTIES UNITY_BUILD OFF)
target_sources(native-layout
  PRIVATE
    src/document.cpp
    src/document_io.cpp
    src/document_file_updates.cpp
    src/document_source_edit.cpp
    src/document_state.cpp
    src/editor_session.cpp
    src/editor_diagnostics.cpp
    src/transaction.cpp
    src/analysis_settings.cpp
    src/abi_profile.cpp
    src/fact_origin.cpp
    src/type_facts.cpp
    src/profile_probe.cpp
    src/graph_view.cpp
    src/analyzer/physical_pass.cpp
    src/analyzer/physical_type_spelling.cpp
    src/analyzer/numeric_delta.cpp
    src/analyzer/scalar_pass.cpp
    src/analyzer/aggregate_pass.cpp
    src/analyzer/packed_pass.cpp
    src/analyzer/soa_pass.cpp
    src/analysis/analyzer.cpp
    src/analysis/compare_runs.cpp
    src/analysis/analysis_context.cpp
    src/analysis/analysis_storage.cpp
    src/analysis/analysis_workloads.cpp
    src/analysis/analysis_distributions.cpp
    src/analysis/analysis_totals.cpp
    src/access_intents.cpp
    src/checked_arithmetic.cpp
    src/enum_backing.cpp
    src/integer_code_domain.cpp
    src/memory_regions.cpp
    src/relationship_extent.cpp
    src/variant_projection.cpp
    src/varint_encoding.cpp
    src/frame_pacer.cpp
    src/planner_controller.cpp
    src/variants.cpp
    src/planner_type.cpp
    src/declaration_kind.cpp
    src/declaration_capabilities.cpp
    src/external_dependency.cpp
    ${layout_analysis_sources}
)
target_include_directories(native-layout
  PUBLIC
    include
    "${layout_analysis_generated}"
)
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
    native-core-headers
  PRIVATE
    lispb-project
)
ioj_enable_warnings(native-layout)

ioj_find_llvm_tool(IOJ_CLANG_FORMAT_EXECUTABLE clang-format)

add_subdirectory(sexpr)
add_subdirectory(material)

add_library(lispb-paths)
if(WIN32)
  target_sources(lispb-paths
    PRIVATE
      src/platform/windows/output_path_key.cpp
  )
else()
  target_sources(lispb-paths
    PRIVATE
      src/platform/posix/output_path_key.cpp
  )
endif()
target_include_directories(lispb-paths PUBLIC include)
target_link_libraries(lispb-paths
  PUBLIC
    sandbox::native_defaults
)
ioj_enable_warnings(lispb-paths CODEGEN)

add_library(lispb-output
  src/output.cpp
  src/compilation.cpp
  src/format_generated.cpp
)
target_compile_definitions(lispb-output
  PRIVATE IOJ_CLANG_FORMAT_EXECUTABLE="${IOJ_CLANG_FORMAT_EXECUTABLE}"
)
target_include_directories(lispb-output PUBLIC include)
target_link_libraries(lispb-output
  PUBLIC
    sandbox::native_defaults
  PRIVATE
    lispb-paths
)
ioj_enable_warnings(lispb-output CODEGEN)

add_library(lispb-project)
target_sources(lispb-project
  PRIVATE
    src/project.cpp
    src/project_loader.cpp
    src/project_parser.cpp
    src/rooted_path.cpp
)
target_include_directories(lispb-project PUBLIC include)
target_link_libraries(lispb-project
  PUBLIC
    sandbox::native_defaults
  PRIVATE
    lispb-paths
    lispb-schema
    lispb-syntax
)
ioj_enable_warnings(lispb-project CODEGEN)

add_library(lispb-schema)
target_sources(lispb-schema PRIVATE
  src/compiler_storage.cpp
  src/complete_dependencies.cpp
  src/ast/cpp_types.cpp
  src/declaration_schema.cpp
  src/declarations.cpp
  src/enumerator_schemas.cpp
  src/enum_schemas.cpp
  src/enum_unreal_projections.cpp
  src/facade_method_schemas.cpp
  src/facade_schemas.cpp
  src/fixed_point_schemas.cpp
  src/function_schemas.cpp
  src/integer_scalar_schemas.cpp
  src/integer_varint_schemas.cpp
  src/linear_quantized_schemas.cpp
  src/mini_float_schemas.cpp
  src/module_settings_store.cpp
  src/normal_module_schemas.cpp
  src/optional_presence_bit_schemas.cpp
  src/optional_sentinel_schemas.cpp
  src/packed_named_code_schemas.cpp
  src/packed_field_schemas.cpp
  src/packed_reserved_bits_schemas.cpp
  src/packed_value_schemas.cpp
  src/parameter_schemas.cpp
  src/record_member_schemas.cpp
  src/record_schemas.cpp
  src/semantic_relation_schemas.cpp
  src/setting_category_schemas.cpp
  src/setting_control_schemas.cpp
  src/setting_schemas.cpp
  src/settings_module_schemas.cpp
  src/soa_mask_dimension_schemas.cpp
  src/soa_member_schemas.cpp
  src/soa_schemas.cpp
  src/static_table_column_schemas.cpp
  src/static_table_group_schemas.cpp
  src/static_table_row_schemas.cpp
  src/static_table_schemas.cpp
  src/tagged_union_alternative_schemas.cpp
  src/tagged_union_schemas.cpp
  src/type_refs.cpp
  src/umbrella_module_schemas.cpp
  src/union_alternative_schemas.cpp
  src/union_schemas.cpp
  src/declaration_query.cpp
  src/document_query.cpp
  src/field_query.cpp
  src/member_query.cpp
  src/packed_value_query.cpp
  src/soa_query.cpp
  src/module_query.cpp
  src/manifest.cpp
  src/packed_integer_value.cpp
  src/semantic_relation_unit.cpp
  src/type_reference_role.cpp
  src/type_registry.cpp
  src/registered_types.cpp
  src/source_renderer.cpp
  src/canonical_renderer.cpp
  src/source_comments.cpp
  src/enum_storage_requirement.cpp
  src/enum_code.cpp
  src/enum_domains.cpp
  src/fixed_point_value.cpp
  src/packed_value_internal.cpp
  src/schema.cpp
  src/physical_type_use.cpp
  src/soa_body.cpp
  src/source_loader.cpp
  src/schema_parser.cpp
  src/integer_domain.cpp
  src/external_types.cpp
  src/enum_types.cpp
  src/integer_scalar_types.cpp
  src/linear_quantized_types.cpp
  src/integer_varint_types.cpp
  src/fixed_point_types.cpp
  src/mini_float_types.cpp
  src/optional_sentinel_types.cpp
  src/optional_presence_bit_types.cpp
  src/packed_types.cpp
  src/record_types.cpp
  src/union_types.cpp
  src/tagged_union_types.cpp
  src/soa_types.cpp
  src/static_table_types.cpp
  src/facade_types.cpp
  src/record_members.cpp
  src/union_alternatives.cpp
  src/tagged_union_alternatives.cpp
  src/soa_columns.cpp
  src/packed_fields.cpp
  src/packed_reserved_segments.cpp
  src/type_graph.cpp
  src/resolved_occurrences.cpp
  src/relationships.cpp
  src/validation.cpp
)
set_target_properties(lispb-schema PROPERTIES UNITY_BUILD OFF)
target_include_directories(lispb-schema PUBLIC include)
target_link_libraries(lispb-schema
  PUBLIC
    sandbox::native_defaults
    lispb-syntax
    native-core-headers
  PRIVATE
    lispb-paths
    lispb-sexpr-output
)
ioj_enable_warnings(lispb-schema CODEGEN)

add_library(codegen-lib)
target_sources(codegen-lib PRIVATE
  src/lowering/lowering_paths.cpp
  src/lowering/lowering_storage.cpp
  src/lowering/single_allocation_columns.cpp
  src/lowering/single_allocation_models.cpp
  src/lowering/packed_field_layouts.cpp
  src/ast/cpp_ir.cpp
  src/ast/cpp_renderer.cpp
  src/ast/cpp_declaration_id.cpp
  src/ast/environment_guard.cpp
  src/ast/environments.cpp
  src/ast/render_state.cpp
  src/ast/cpp_files.cpp
  src/ast/modules.cpp
  src/ast/render_context.cpp
  src/ast/include.cpp
  src/ast/node_list_builder.cpp
  src/cpp_string.cpp
  src/lowering_context.cpp
  src/enum_lowering.cpp
  src/facade_lowering.cpp
  src/generator.cpp
  src/lowering_utils.cpp
  src/module_assembly.cpp
  src/settings_lowering.cpp
  src/string_enum_parser.cpp
  src/soa_lowering.cpp
  src/soa_api.cpp
  src/single_allocation_soa_emission.cpp
  src/single_allocation_soa_lowering.cpp
  src/single_allocation_soa_model.cpp
  src/packed_value_lowering.cpp
  src/record_lowering.cpp
  src/record_reader_lowering.cpp
  src/union_lowering.cpp
  src/static_table_lowering.cpp
)
set_target_properties(codegen-lib PROPERTIES UNITY_BUILD OFF)
target_include_directories(codegen-lib PUBLIC include)
target_link_libraries(codegen-lib
  PUBLIC
    sandbox::native_defaults
    lispb-output
    lispb-schema
    native-core-headers
  PRIVATE
    lispb-paths
    ioj::codegen_pch
)
ioj_enable_warnings(codegen-lib CODEGEN)

add_library(lispb-target-compiler
  src/compiler.cpp
  src/material_compiler.cpp
)
target_include_directories(lispb-target-compiler PUBLIC include)
target_link_libraries(lispb-target-compiler
  PUBLIC
    sandbox::native_defaults
    codegen-lib
    kernel-codegen-lib
    lispb-project
    material-gen
    slate-codegen-lib
)
ioj_enable_warnings(lispb-target-compiler CODEGEN)

add_executable(lispb src/main.cpp)
target_link_libraries(lispb PRIVATE
  lispb-target-compiler
  sandbox::native_defaults
)
ioj_enable_warnings(lispb CODEGEN)

set(CODEGEN_COMPILE_FIXTURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fixture")
set(CODEGEN_COMPILE_FIXTURE_OUTPUT "${CODEGEN_COMPILE_FIXTURE_SOURCE}/generated")
set(CODEGEN_COMPILE_FIXTURE_HEADERS
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Records.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/NestedSettings.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/NestedSettings_reader.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/FieldMasks.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/RestrictionRows.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/ReferenceFacade.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Generated.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Mixed.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureFirst.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureSecond.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureDefinitions.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/InlineFacade.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SourceFacade.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PlainEnums.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PackedEnums.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/ScalarValues.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/Representations.h"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PackedValues.h"
)
set(CODEGEN_COMPILE_FIXTURE_SOURCES
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureFirst.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureSecond.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SignatureDefinitions.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/SourceFacade.cpp"
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}/PlainEnums.cpp"
)
add_custom_target(generate-codegen-compile-fixture
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target cpp-compile-fixture
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed C++ codegen fixture"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(check-generated-codegen-compile-fixture
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target cpp-compile-fixture
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed C++ codegen fixture"
  USES_TERMINAL
  VERBATIM
)
add_library(codegen-generated-compile-fixture OBJECT)
target_sources(codegen-generated-compile-fixture PRIVATE
  ${CODEGEN_COMPILE_FIXTURE_HEADERS}
  ${CODEGEN_COMPILE_FIXTURE_SOURCES}
)
set_target_properties(codegen-generated-compile-fixture PROPERTIES UNITY_BUILD OFF)
target_include_directories(codegen-generated-compile-fixture PUBLIC
  "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
  "${CODEGEN_COMPILE_FIXTURE_SOURCE}/include"
  "${PROJECT_SOURCE_DIR}/native/core/include"
)
target_link_libraries(codegen-generated-compile-fixture PRIVATE sandbox::native_defaults)
ioj_enable_warnings(codegen-generated-compile-fixture CODEGEN)

add_executable(codegen-generated-tests)
target_sources(codegen-generated-tests PRIVATE
  tests/generated_tests.cpp
  tests/packed_support_tests.cpp
  tests/generated_reader_tests.cpp
)
set_target_properties(codegen-generated-tests PROPERTIES UNITY_BUILD OFF)
target_link_libraries(codegen-generated-tests PRIVATE
  codegen-generated-compile-fixture
  sandbox-s7
  GTest::gtest_main
  sandbox::native_defaults
)
ioj_enable_warnings(codegen-generated-tests CODEGEN)

add_executable(codegen-tests
  tests/test_utilities.cpp
  tests/ast_tests.cpp
  tests/environment_tests.cpp
  tests/expr_tests.cpp
  tests/single_allocation_soa_tests.cpp
  tests/generator_tests.cpp
  tests/source_loader_tests.cpp
  tests/lowering_tests.cpp
  tests/packed_value_tests.cpp
  tests/project_tests.cpp
  tests/record_lowering_tests.cpp
  tests/union_lowering_tests.cpp
  tests/semantic_model_tests.cpp
  tests/static_table_lowering_tests.cpp
  tests/validation_tests.cpp
)
set_target_properties(codegen-tests PROPERTIES UNITY_BUILD OFF)
add_dependencies(codegen-tests
  codegen-generated-tests
  lispb-syntax-tests
  material-gen-tests
)
target_link_libraries(codegen-tests PRIVATE
  lispb-target-compiler
  GTest::gtest_main
  sandbox::native_defaults
  ioj::test_pch
)
ioj_enable_warnings(codegen-tests CODEGEN)
target_compile_definitions(codegen-tests PRIVATE
  IOJ_CODEGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME codegen-tests COMMAND codegen-tests)
set_tests_properties(codegen-tests PROPERTIES LABELS "unit;all;native;codegen")
add_test(NAME codegen-generated-tests COMMAND codegen-generated-tests)
set_tests_properties(codegen-generated-tests PROPERTIES LABELS "unit;all;native;codegen")
add_library(codegen-reject-unsupported-reader OBJECT EXCLUDE_FROM_ALL)
target_sources(codegen-reject-unsupported-reader
  PRIVATE
    tests/compile_fixture/unsupported_reader.cpp
)
target_link_libraries(codegen-reject-unsupported-reader
  PRIVATE
    codegen-generated-compile-fixture
    native-s7-data
    sandbox::native_defaults
)
ioj_enable_warnings(codegen-reject-unsupported-reader CODEGEN)
add_test(NAME S7ReaderRejectsUnsupportedMember
  COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
    -DREJECT_TARGET=codegen-reject-unsupported-reader
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reject_unsupported_reader.cmake"
)
set_tests_properties(S7ReaderRejectsUnsupportedMember PROPERTIES
  LABELS "all;native;codegen;compile-contract;subprocess"
  RESOURCE_LOCK codegen-negative-build
)
foreach(restriction IN ITEMS COPY DESTRUCTOR DEFAULT)
  set(reject_target "codegen-reject-single-${restriction}")
  add_library(${reject_target} OBJECT EXCLUDE_FROM_ALL tests/compile_fixture/rejected_leaf.cpp)
  add_dependencies(${reject_target} codegen-generated-compile-fixture)
  target_compile_definitions(${reject_target} PRIVATE "CODEGEN_BAD_${restriction}")
  target_include_directories(${reject_target} PRIVATE
    "${CODEGEN_COMPILE_FIXTURE_OUTPUT}"
    "${CODEGEN_COMPILE_FIXTURE_SOURCE}/include"
    "${PROJECT_SOURCE_DIR}/native/core/include"
    )
  target_link_libraries(${reject_target} PRIVATE sandbox::native_defaults)
  ioj_enable_warnings(${reject_target} CODEGEN)
  add_test(NAME "SingleAllocationRejects${restriction}"
    COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DREJECT_TARGET=${reject_target}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reject_single_allocation_leaf.cmake")
  set_tests_properties("SingleAllocationRejects${restriction}" PROPERTIES
    LABELS "all;native;codegen;compile-contract;subprocess" RESOURCE_LOCK codegen-negative-build)
endforeach()
add_test(NAME CodegenCliRejectsUnknownArgument COMMAND lispb generate --unknown value)
set_tests_properties(CodegenCliRejectsUnknownArgument PROPERTIES
  LABELS "unit;all;native;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsMissingArgumentValue COMMAND lispb generate --target)
set_tests_properties(CodegenCliRejectsMissingArgumentValue PROPERTIES
  LABELS "unit;all;native;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsOptionAsArgumentValue
  COMMAND lispb generate --project --target
)
set_tests_properties(CodegenCliRejectsOptionAsArgumentValue PROPERTIES
  LABELS "unit;all;native;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliRejectsDuplicateArguments
  COMMAND lispb generate --target sandbox-code --target sandbox-code
)
set_tests_properties(CodegenCliRejectsDuplicateArguments PROPERTIES
  LABELS "unit;all;native;codegen"
  WILL_FAIL TRUE
)
add_test(NAME CodegenCliReportsMissingProject
  COMMAND lispb generate --project missing-project.lispb --target sandbox-code
)
set_tests_properties(CodegenCliReportsMissingProject PROPERTIES
  LABELS "unit;all;native;codegen"
  WILL_FAIL TRUE
)

add_custom_target(generate-code
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target compiler-ir
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target compiler-schema
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target sandbox-code
  COMMAND "$<TARGET_FILE:lispb>" generate
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(check-generated-code
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target compiler-ir
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target compiler-schema
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target sandbox-code
  COMMAND "$<TARGET_FILE:lispb>" check
    --project "${PROJECT_SOURCE_DIR}/lispb/project.lispb"
    --build-root "${CMAKE_BINARY_DIR}"
    --target native-soa-fixture
  DEPENDS lispb
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking committed Sandbox C++ files"
  USES_TERMINAL
  VERBATIM
)
add_dependencies(check-generated-code check-generated-codegen-compile-fixture)

add_subdirectory(slate)
add_subdirectory(kernel)
add_subdirectory(tests/native_soa)
add_subdirectory(benchmarks/soa)

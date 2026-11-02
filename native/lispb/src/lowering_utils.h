#include <codegen/ast/indices.h>
#pragma once

#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/node_id.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/type_registry.h>
#include <lispb/indices.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {

auto enum_count_specialization(LoweringContext& ctx, CppTypeId enum_type, std::size_t count)
    -> NodeId;
auto enum_strings_specialization(LoweringContext& ctx,
                                 std::string_view qualified,
                                 std::string_view category,
                                 std::span<std::string const> names,
                                 bool wide = false) -> NodeId;

auto lower_schema_function(LoweringContext& ctx,
                           FunctionSchemaId schema,
                           std::optional<Nodes> body = std::nullopt,
                           bool inline_header_definition = false) -> FunctionDefinition;
auto lower_member_type(LoweringContext& ctx, ChildOccurrenceId member) -> CppTypeId;

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string;
auto join(LoweringContext const& ctx, SchemaRange<StringId> values, std::string_view separator)
    -> std::string;
auto join_lines(std::vector<std::string> const& values) -> std::string;
auto join_lines(LoweringContext const& ctx, SchemaRange<StringId> values) -> std::string;
auto source_include(LoweringContext const& ctx, ModuleSettingsId settings) -> std::string;
void append_type_dependencies(std::vector<TypeDependencyId>& result,
                              std::string const& key,
                              TypeRegistry const& types);
auto title_case_identifier(std::string_view const identifier) -> std::string;
auto snake_case_type_name(std::string_view name) -> std::string;
auto column_apply_arrays_function(LoweringContext& ctx, std::vector<std::string> const& columns)
    -> NodeId;
auto column_apply_array_pairs_function(LoweringContext& ctx,
                                       std::vector<std::string> const& columns) -> NodeId;

} // namespace lispb::codegen::detail

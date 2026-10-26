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

auto enum_count_specialization(CppIR& ir, CppTypeId enum_type, std::size_t count) -> NodeId;
auto enum_strings_specialization(CppIR& ir,
                                 std::string_view qualified,
                                 std::string_view category,
                                 std::span<std::string const> names,
                                 bool wide = false) -> NodeId;

auto lower_schema_function(LoweringContext& context,
                           FunctionSchemaId schema,
                           std::optional<std::string_view> body = std::nullopt) -> FunctionId;

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string;
auto join(LoweringContext const& context, SchemaRange<StringId> values, std::string_view separator)
    -> std::string;
auto join_lines(std::vector<std::string> const& values) -> std::string;
auto join_lines(LoweringContext const& context, SchemaRange<StringId> values) -> std::string;
auto source_include(LoweringContext const& context, ModuleSettingsId settings) -> std::string;
void append_type_dependencies(std::vector<TypeDependencyId>& result,
                              std::string const& key,
                              TypeRegistry const& types);
auto qualify(CppIR& ir, CppTypeId type, std::string const& suffix) -> CppTypeId;
auto title_case_identifier(std::string_view const identifier) -> std::string;
auto column_apply_arrays_function(CppIR& ir, std::vector<std::string> const& columns) -> NodeId;
auto column_apply_array_pairs_function(CppIR& ir, std::vector<std::string> const& columns)
    -> NodeId;

} // namespace lispb::codegen::detail

#pragma once

#include "compiler_test_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/generated_file.h>
#include <codegen/generator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/type_registry.h>
#include <codegen/source_loader.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

template <typename Id>
inline auto test_text(Strings const& strings, Id const id) -> std::string_view {
    return strings.view(id);
}

template <typename Id>
inline auto test_text(Strings const& strings, std::optional<Id> const id)
    -> std::optional<std::string_view> {
    return id ? std::optional{strings.view(*id)} : std::nullopt;
}

template <typename Id>
inline auto test_items(Declarations const& declarations, SchemaRange<Id> const range) {
    auto const items{declarations.items(range)};
    if constexpr (std::same_as<Id, IdentifierId> || std::same_as<Id, StringId>) {
        std::vector<std::string> result;
        for (auto const item : items) {
            result.emplace_back(declarations.strings().view(item));
        }
        return result;
    } else {
        return std::vector<Id>{items.begin(), items.end()};
    }
}

auto make_test_registry(CompilerTestContext& context,
                        std::vector<std::pair<std::string_view, CppTypeId>> const& entries = {})
    -> TypeRegistry;

auto lower_test_manifest(CompilerTestContext& context, Manifest manifest) -> std::vector<ModuleId>;

auto parse_test_manifest(
    CompilerTestContext& context,
    std::string_view source,
    std::vector<std::pair<std::string_view, CppTypeId>> const& registrations = {}) -> Manifest;

auto find_test_type(schema::TypeGraph const& graph, std::string_view module, std::string_view name)
    -> std::optional<schema::TypeId>;

auto render_test_manifest(CompilerTestContext& context, Manifest manifest)
    -> std::vector<GeneratedFile>;

auto collected_dependencies(CppIR const& ir, NodeId node) -> std::vector<TypeDependencyId>;

auto collected_dependencies(CppIR const& ir, Expr expression) -> std::vector<TypeDependencyId>;

auto occurrences(std::string const& text, std::string const& value) -> std::size_t;

}

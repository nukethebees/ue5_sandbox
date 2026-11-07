#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace lispb::schema {
class TypeGraph;
}
namespace lispb::codegen {
class Declarations;
struct Manifest;
}

namespace lispb::codegen::detail {

auto packed_unsigned_width(std::string_view spelling) -> std::optional<int>;
auto packed_signed_width(std::string_view spelling) -> std::optional<int>;
auto find_integer_domain(TypeRefId reference, schema::TypeGraph const& graph, ModuleHandle module)
    -> std::optional<schema::TypeDefinitionId>;
auto find_packed_enum(TypeRefId reference, schema::TypeGraph const& graph, ModuleHandle module)
    -> std::optional<lispb::codegen::DeclarationHandle>;
auto find_linear_quantized(TypeRefId reference, schema::TypeGraph const& graph, ModuleHandle module)
    -> std::optional<lispb::codegen::DeclarationHandle>;
auto find_fixed_point(TypeRefId reference, schema::TypeGraph const& graph, ModuleHandle module)
    -> std::optional<lispb::codegen::DeclarationHandle>;
auto find_mini_float(TypeRefId reference, schema::TypeGraph const& graph, ModuleHandle module)
    -> std::optional<lispb::codegen::DeclarationHandle>;

} // namespace lispb::codegen::detail

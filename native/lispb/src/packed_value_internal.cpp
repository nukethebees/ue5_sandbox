#include "packed_value_internal.h"

#include <codegen/fixed_width_integer_types.h>
#include <lispb/schema/type_graph.h>

#include <optional>
#include <string_view>

namespace lispb::codegen::detail {
namespace {
auto referenced_declaration(TypeRefId reference,
                            lispb::schema::TypeGraph const& graph,
                            ModuleHandle module,
                            DeclarationKind kind) -> std::optional<DeclarationHandle> {
    auto const occurrence{graph.resolved(module, reference)};
    auto const& occurrences{graph.occurrences()};
    auto const source{graph.source_declaration(occurrences.type[occurrences.index(occurrence)])};
    return source.value != 0 && source.kind() == kind ? std::optional{source} : std::nullopt;
}
} // namespace

auto packed_unsigned_width(std::string_view const spelling) -> std::optional<int> {
    for (auto const& type : fixed_width_integer_types) {
        if (!type.is_signed && (spelling == type.name || spelling == type.native_name)) {
            return type.width;
        }
    }
    return std::nullopt;
}
auto packed_signed_width(std::string_view const spelling) -> std::optional<int> {
    for (auto const& type : fixed_width_integer_types) {
        if (type.is_signed && (spelling == type.name || spelling == type.native_name)) {
            return type.width;
        }
    }
    return std::nullopt;
}

auto find_packed_enum(TypeRefId reference,
                      lispb::schema::TypeGraph const& graph,
                      ModuleHandle module) -> std::optional<lispb::codegen::DeclarationHandle> {
    return referenced_declaration(reference, graph, module, DeclarationKind::enumeration);
}

auto find_linear_quantized(TypeRefId reference,
                           lispb::schema::TypeGraph const& graph,
                           ModuleHandle module)
    -> std::optional<lispb::codegen::DeclarationHandle> {
    return referenced_declaration(reference, graph, module, DeclarationKind::linear_quantized);
}

auto find_fixed_point(TypeRefId reference,
                      lispb::schema::TypeGraph const& graph,
                      ModuleHandle module) -> std::optional<lispb::codegen::DeclarationHandle> {
    return referenced_declaration(reference, graph, module, DeclarationKind::fixed_point);
}

auto find_mini_float(TypeRefId reference,
                     lispb::schema::TypeGraph const& graph,
                     ModuleHandle module) -> std::optional<lispb::codegen::DeclarationHandle> {
    return referenced_declaration(reference, graph, module, DeclarationKind::mini_float);
}

}

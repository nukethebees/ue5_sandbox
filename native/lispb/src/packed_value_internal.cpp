#include "packed_value_internal.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_point_value.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/type_refs.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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
    constexpr std::array values{std::pair{"uint8", 8},
                                std::pair{"uint16", 16},
                                std::pair{"uint32", 32},
                                std::pair{"uint64", 64},
                                std::pair{"std::uint8_t", 8},
                                std::pair{"std::uint16_t", 16},
                                std::pair{"std::uint32_t", 32},
                                std::pair{"std::uint64_t", 64}};
    for (auto const& [name, width] : values) {
        if (spelling == name) {
            return width;
        }
    }
    return std::nullopt;
}
auto packed_signed_width(std::string_view const spelling) -> std::optional<int> {
    constexpr std::array values{std::pair{"int8", 8},
                                std::pair{"int16", 16},
                                std::pair{"int32", 32},
                                std::pair{"int64", 64},
                                std::pair{"std::int8_t", 8},
                                std::pair{"std::int16_t", 16},
                                std::pair{"std::int32_t", 32},
                                std::pair{"std::int64_t", 64}};
    for (auto const& [name, width] : values) {
        if (spelling == name) {
            return width;
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

auto find_integer_domain(TypeRefId const reference,
                         lispb::schema::TypeGraph const& graph,
                         ModuleHandle const module) -> std::optional<schema::TypeDefinitionId> {
    auto const& declarations{graph.declarations()};
    auto const& references{declarations.type_refs};
    auto const row{references.index(reference)};
    if (!declarations.strings().view(references.suffix[row]).empty() || references.nested[row]) {
        return std::nullopt;
    }
    auto const occurrence{graph.resolved(module, reference)};
    auto const& occurrences{graph.occurrences()};
    return schema::integer_domain(graph, occurrences.type[occurrences.index(occurrence)]);
}

}

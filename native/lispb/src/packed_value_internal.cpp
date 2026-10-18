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
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
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

auto find_packed_integer_domain(PackedSegmentId const field,
                                lispb::schema::TypeGraph const& graph,
                                ModuleHandle const module)
    -> std::optional<schema::TypeDefinitionId> {
    auto const& declarations{graph.declarations()};
    auto const& fields{declarations.packed_fields};
    auto const reference{fields.type[fields.index(field)]};
    auto const& references{declarations.type_refs};
    auto const row{references.index(reference)};
    if (!declarations.strings().view(references.suffix[row]).empty() || references.nested[row]) {
        return std::nullopt;
    }
    auto const occurrence{graph.resolved(module, reference)};
    auto const& occurrences{graph.occurrences()};
    return schema::packed_integer_domain(
        graph, occurrences.type[occurrences.index(occurrence)], field);
}

auto derive_packed_field_width(lispb::codegen::PackedSegmentId field,
                               Manifest const& manifest,
                               lispb::schema::TypeGraph const& graph,
                               ModuleHandle module) -> std::optional<int> {
    if (manifest.declarations.packed_fields.bits[manifest.declarations.packed_fields.index(field)]
            .has_value()) {
        return manifest.declarations.packed_fields
            .bits[manifest.declarations.packed_fields.index(field)];
    }
    if (auto const scalar{
            find_integer_domain(manifest.declarations.packed_fields
                                    .type[manifest.declarations.packed_fields.index(field)],
                                graph,
                                module)}) {
        auto const& scalars{graph.definitions().integer_scalars};
        return static_cast<int>(scalars.bit_width[scalars.index(*scalar)]);
    }
    if (manifest.declarations.packed_fields
            .kind[manifest.declarations.packed_fields.index(field)] ==
        PackedFieldKind::linear_quantized) {
        auto const quantized{
            find_linear_quantized(manifest.declarations.packed_fields
                                      .type[manifest.declarations.packed_fields.index(field)],
                                  graph,
                                  module)};
        return quantized.has_value()
                 ? std::optional<int>{static_cast<int>(
                       manifest.declarations.linear_quantizeds
                           .bit_width[manifest.declarations.linear_quantizeds.index(*quantized)])}
                 : std::nullopt;
    }
    if (manifest.declarations.packed_fields
            .kind[manifest.declarations.packed_fields.index(field)] ==
        PackedFieldKind::fixed_point) {
        auto const fixed{
            find_fixed_point(manifest.declarations.packed_fields
                                 .type[manifest.declarations.packed_fields.index(field)],
                             graph,
                             module)};
        return fixed.has_value()
                 ? std::optional<int>{static_cast<int>(
                       manifest.declarations.fixed_points
                           .total_bits[manifest.declarations.fixed_points.index(*fixed)])}
                 : std::nullopt;
    }
    if (manifest.declarations.packed_fields
            .kind[manifest.declarations.packed_fields.index(field)] ==
        PackedFieldKind::mini_float) {
        auto const mini{find_mini_float(manifest.declarations.packed_fields
                                            .type[manifest.declarations.packed_fields.index(field)],
                                        graph,
                                        module)};
        return mini.has_value()
                 ? std::optional<int>{static_cast<int>(
                       manifest.declarations.mini_floats
                           .sign_bits[manifest.declarations.mini_floats.index(*mini)] +
                       manifest.declarations.mini_floats
                           .exponent_bits[manifest.declarations.mini_floats.index(*mini)] +
                       manifest.declarations.mini_floats
                           .significand_bits[manifest.declarations.mini_floats.index(*mini)])}
                 : std::nullopt;
    }
    if (manifest.declarations.packed_fields
            .kind[manifest.declarations.packed_fields.index(field)] ==
        PackedFieldKind::enumeration) {
        auto const enumeration{
            find_packed_enum(manifest.declarations.packed_fields
                                 .type[manifest.declarations.packed_fields.index(field)],
                             graph,
                             module)};
        if (!enumeration.has_value()) {
            return std::nullopt;
        }
        if (manifest.declarations.enums.bit_width[manifest.declarations.enums.index(*enumeration)]
                .has_value()) {
            return static_cast<int>(
                *manifest.declarations.enums
                     .bit_width[manifest.declarations.enums.index(*enumeration)]);
        }
        auto const& domains{graph.enum_domains()};
        auto const domain{domains.find(*enumeration)};
        auto const bits{domains.minimum_required_bits[domains.index(domain)]};
        return bits ? std::optional<int>{static_cast<int>(*bits)} : std::nullopt;
    }
    if (!manifest.declarations.packed_fields
             .minimum_value[manifest.declarations.packed_fields.index(field)]
             .has_value() ||
        !manifest.declarations.packed_fields
             .maximum_value[manifest.declarations.packed_fields.index(field)]
             .has_value()) {
        return std::nullopt;
    }

    auto minimum_code{*manifest.declarations.packed_fields
                           .minimum_value[manifest.declarations.packed_fields.index(field)]};
    auto maximum_code{*manifest.declarations.packed_fields
                           .maximum_value[manifest.declarations.packed_fields.index(field)]};
    for (auto const id : manifest.declarations.items(
             manifest.declarations.packed_fields
                 .named_codes[manifest.declarations.packed_fields.index(field)])) {
        auto const code{id};
        if (packed_integer_less(manifest.declarations.packed_named_codes
                                    .value[manifest.declarations.packed_named_codes.index(code)],
                                minimum_code)) {
            minimum_code = manifest.declarations.packed_named_codes
                               .value[manifest.declarations.packed_named_codes.index(code)];
        }
        if (packed_integer_less(maximum_code,
                                manifest.declarations.packed_named_codes
                                    .value[manifest.declarations.packed_named_codes.index(code)])) {
            maximum_code = manifest.declarations.packed_named_codes
                               .value[manifest.declarations.packed_named_codes.index(code)];
        }
    }
    auto const bits{minimum_packed_integer_bits(
        minimum_code,
        maximum_code,
        manifest.declarations.packed_fields
                .kind[manifest.declarations.packed_fields.index(field)] ==
            PackedFieldKind::signed_integer)};
    return bits.has_value() ? std::optional<int>{static_cast<int>(*bits)} : std::nullopt;
}

auto packed_default_value(lispb::codegen::DeclarationHandle schema,
                          Manifest const& manifest,
                          lispb::schema::TypeGraph const& graph,
                          ModuleHandle module) -> std::optional<std::uint64_t> {
    auto const& integer_scalars{graph.definitions().integer_scalars};
    auto const storage{
        graph.resolved(module,
                       manifest.declarations.packed_values
                           .storage_type[manifest.declarations.packed_values.index(schema)])};
    auto const& occurrences{graph.occurrences()};
    auto const storage_bits{
        packed_unsigned_width(
            graph.cpp_types().spelling(occurrences.cpp_type[occurrences.index(storage)]))
            .value()};
    auto const most_significant_first{
        manifest.declarations.packed_values
            .bit_order[manifest.declarations.packed_values.index(schema)] ==
        PackedBitOrder::most_significant_first};
    auto used_bits{0};
    auto any_default{false};
    auto complete{true};
    std::uint64_t raw{};

    for (auto const segment : manifest.declarations.items(
             manifest.declarations.packed_values
                 .segments[manifest.declarations.packed_values.index(schema)])) {
        auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                              ? std::optional{segment}
                              : std::nullopt)};
        auto const bits{field.has_value()
                            ? derive_packed_field_width(*field, manifest, graph, module).value()
                            : manifest.declarations.packed_reserved_bits
                                  .bits[manifest.declarations.packed_reserved_bits.index(segment)]};
        auto const offset{most_significant_first ? storage_bits - used_bits - bits : used_bits};
        used_bits += bits;
        if (!field.has_value()) {
            continue;
        }
        if (!manifest.declarations.packed_fields
                 .default_value[manifest.declarations.packed_fields.index(*field)]
                 .has_value()) {
            complete = false;
            continue;
        }

        any_default = true;
        auto const value{*manifest.declarations.packed_fields
                              .default_value[manifest.declarations.packed_fields.index(*field)]};
        auto const scalar{find_packed_integer_domain(*field, graph, module)};
        auto const fixed{
            find_fixed_point(manifest.declarations.packed_fields
                                 .type[manifest.declarations.packed_fields.index(*field)],
                             graph,
                             module)};
        auto const signedness{
            fixed.has_value() ? manifest.declarations.fixed_points
                                    .signedness[manifest.declarations.fixed_points.index(*fixed)]
            : scalar.has_value()
                ? integer_scalars.signedness[integer_scalars.index(*scalar)]
                : manifest.declarations.packed_fields
                          .kind[manifest.declarations.packed_fields.index(*field)] ==
                      PackedFieldKind::signed_integer};
        auto valid{signedness
                       ? packed_integer_fits_signed(value, static_cast<std::uint32_t>(bits))
                       : packed_integer_fits_unsigned(value, static_cast<std::uint32_t>(bits))};
        auto const minimum{
            scalar.has_value()
                ? std::optional{integer_scalars.minimum_value[integer_scalars.index(*scalar)]}
                : manifest.declarations.packed_fields
                      .minimum_value[manifest.declarations.packed_fields.index(*field)]};
        auto const maximum{
            scalar.has_value()
                ? std::optional{integer_scalars.maximum_value[integer_scalars.index(*scalar)]}
                : manifest.declarations.packed_fields
                      .maximum_value[manifest.declarations.packed_fields.index(*field)]};
        auto const codes{scalar.has_value()
                             ? integer_scalars.named_codes[integer_scalars.index(*scalar)]
                             : manifest.declarations.packed_fields
                                   .named_codes[manifest.declarations.packed_fields.index(*field)]};
        if (minimum.has_value() && maximum.has_value()) {
            auto const sentinel{
                std::ranges::any_of(manifest.declarations.items(codes), [&](auto const id) {
                    auto const code{id};
                    return manifest.declarations.packed_named_codes
                               .sentinel[manifest.declarations.packed_named_codes.index(code)] &&
                           manifest.declarations.packed_named_codes
                                   .value[manifest.declarations.packed_named_codes.index(code)] ==
                               value;
                })};
            valid = valid && (sentinel || (packed_integer_less_equal(*minimum, value) &&
                                           packed_integer_less_equal(value, *maximum)));
        }
        if (manifest.declarations.packed_fields
                .kind[manifest.declarations.packed_fields.index(*field)] ==
            PackedFieldKind::enumeration) {
            auto const enumeration{
                find_packed_enum(manifest.declarations.packed_fields
                                     .type[manifest.declarations.packed_fields.index(*field)],
                                 graph,
                                 module)};
            if (!enumeration.has_value()) {
                valid = false;
            } else {
                auto const& domains{graph.enum_domains()};
                auto const domain{domains.find(*enumeration)};
                auto const values{domains.values(domain)};
                auto const count{manifest.declarations.enums
                                     .count[manifest.declarations.enums.index(*enumeration)]};
                auto const& enumerators{manifest.declarations.enumerators};
                bool known_live_value{};
                auto const size{values.size()};
                for (std::size_t ordinal{}; ordinal < size; ++ordinal) {
                    auto const row{enumerators.index(values[ordinal])};
                    auto const code{domains.code(domain, ordinal)};
                    if (!enumerators.sentinel[row] && count != enumerators.name[row] && code &&
                        code->negative == value.negative && code->magnitude == value.magnitude) {
                        known_live_value = true;
                        break;
                    }
                }
                valid = valid && known_live_value;
            }
        }
        if (auto const quantized{
                find_linear_quantized(manifest.declarations.packed_fields
                                          .type[manifest.declarations.packed_fields.index(*field)],
                                      graph,
                                      module)}) {
            auto const mask{bits == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                       : (std::uint64_t{1} << bits) - 1};
            valid = valid && !value.negative &&
                    value.magnitude <=
                        mask - manifest.declarations.linear_quantizeds
                                   .reserved_codes[manifest.declarations.linear_quantizeds.index(
                                       *quantized)];
        }
        if (fixed.has_value()) {
            if (manifest.declarations.fixed_points
                    .minimum_value[manifest.declarations.fixed_points.index(*fixed)]
                    .has_value()) {
                auto const bound{parse_fixed_point_value(
                    manifest.declarations.strings().view(
                        *manifest.declarations.fixed_points
                             .minimum_value[manifest.declarations.fixed_points.index(*fixed)]),
                    manifest.declarations.fixed_points
                        .fractional_bits[manifest.declarations.fixed_points.index(*fixed)])};
                valid = valid && bound.has_value() && packed_integer_less_equal(*bound, value);
            }
            if (manifest.declarations.fixed_points
                    .maximum_value[manifest.declarations.fixed_points.index(*fixed)]
                    .has_value()) {
                auto const bound{parse_fixed_point_value(
                    manifest.declarations.strings().view(
                        *manifest.declarations.fixed_points
                             .maximum_value[manifest.declarations.fixed_points.index(*fixed)]),
                    manifest.declarations.fixed_points
                        .fractional_bits[manifest.declarations.fixed_points.index(*fixed)])};
                valid = valid && bound.has_value() && packed_integer_less_equal(value, *bound);
            }
        }
        if (!valid) {
            throw std::invalid_argument{
                "Packed value '" +
                std::string{manifest.declarations.strings().view(
                    manifest.declarations.packed_values
                        .name[manifest.declarations.packed_values.index(schema)])} +
                "' field '" +
                std::string{manifest.declarations.strings().view(
                    manifest.declarations.packed_fields
                        .name[manifest.declarations.packed_fields.index(*field)])} +
                "' default is outside its representable semantic domain"};
        }
        auto const mask{bits == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                   : (std::uint64_t{1} << bits) - 1};
        auto const encoded{value.negative ? std::uint64_t{0} - value.magnitude : value.magnitude};
        raw |= (encoded & mask) << offset;
    }

    if (!any_default) {
        return manifest.declarations.packed_values
            .invalid_value[manifest.declarations.packed_values.index(schema)]
            .value_or(0);
    }
    if (!complete) {
        return std::nullopt;
    }
    if (manifest.declarations.packed_values
            .invalid_value[manifest.declarations.packed_values.index(schema)] == raw) {
        throw std::invalid_argument{
            "Packed value '" +
            std::string{manifest.declarations.strings().view(
                manifest.declarations.packed_values
                    .name[manifest.declarations.packed_values.index(schema)])} +
            "' defaults produce its invalid value"};
    }
    return raw;
}

} // namespace lispb::codegen::detail

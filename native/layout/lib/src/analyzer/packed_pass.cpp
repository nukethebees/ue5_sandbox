#include "packed_pass.hpp"

#include "../access_intents.hpp"
#include "../analysis_limits.hpp"
#include "../checked_arithmetic.hpp"
#include "../integer_code_domain.hpp"
#include "../memory_regions.hpp"
#include "../relationship_extent.hpp"
#include "../variant_projection.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/physical_type_spelling.hpp>
#include <ioj/layout/resolved_variant.hpp>
#include <ioj/layout/string_set.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <lispb/project_loader.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>

namespace ioj::layout::analysis_detail {
void
    analyze_packed_layout(AnalysisContext& context, AnalysisId id, ResolvedVariant const& variant) {
    auto const& types{context.types};
    auto const type{context.storage.types.schema_type[id.value]};

    auto const packed{types.definition(type)};
    auto const& packed_table{types.definitions().packed};
    auto const& packed_fields{types.definitions().packed_fields};
    auto const& packed_reserved_bits{types.definitions().packed_reserved_bits};
    auto const packed_row{packed_table.index(packed)};
    auto const schema_storage{
        physical_type_spelling(types, packed_table.storage_type[packed_row])
            .value_or(std::string{types.cpp_types().spelling(
                types.occurrences()
                    .cpp_type[types.occurrences().index(packed_table.storage_type[packed_row])])})};
    auto const storage_type{effective_storage_type(types, type, packed, variant)};
    PackedLayout result{.schema_storage_type = context.storage.strings.intern(schema_storage),
                        .storage_type = context.storage.strings.intern(storage_type),
                        .storage_overridden = variant.overrides.packed_storage_types.contains(type),
                        .byte_order = packed_table.byte_order[packed_row],
                        .bit_order = packed_table.bit_order[packed_row],
                        .invalid_raw_value = packed_table.invalid_raw_value[packed_row]};
    auto const facts{context.storage.physical_facts.get_const_view().resolve()};
    result.storage_facts = context.storage.types.physical[id.value];
    if (!result.storage_facts.has_value()) {
        context.report({DiagnosticSeverity::warning,
                        "Unknown physical facts for packed storage type '" + storage_type + "'."});
    } else {
        result.storage_bits = checked_multiply(facts.size_bytes()[result.storage_facts->value], 8);
        if (!result.storage_bits.has_value()) {
            context.report(
                {DiagnosticSeverity::error, "Packed storage bit count overflows uint64."});
        }
        if (!facts.unsigned_value_bits()[result.storage_facts->value].has_value()) {
            context.report({DiagnosticSeverity::warning,
                            "Packed storage type '" + storage_type +
                                "' is not described as an unsigned integer by this ABI profile."});
        }
    }

    std::uint64_t offset{};
    std::uint64_t payload_bits{};
    std::uint64_t reserved_bits{};
    bool width_overflow{};
    auto& fields{context.storage.fields};
    result.fields.begin = fields.parent.size();
    for (auto const& segment : types.definitions().packed.segments_of(packed)) {
        auto const field{((segment).kind() == lispb::schema::PackedSegmentKind::field
                              ? std::optional{segment}
                              : std::nullopt)};
        auto const reserved{((segment).kind() == lispb::schema::PackedSegmentKind::reserved
                                 ? std::optional{segment}
                                 : std::nullopt)};
        auto const schema_field_row{field ? packed_fields.index(*field) : 0};
        auto const reserved_row{reserved ? packed_reserved_bits.index(*reserved) : 0};
        auto const field_override{
            field.has_value() &&
            variant.overrides.packed_field_widths.contains(
                FieldOverrideId{.type = type, .field_name = packed_fields.name[schema_field_row]})};
        auto const representation_owned_width{
            field.has_value() &&
            (packed_fields.kind[schema_field_row] ==
                 lispb::codegen::PackedFieldKind::linear_quantized ||
             packed_fields.kind[schema_field_row] == lispb::codegen::PackedFieldKind::fixed_point ||
             packed_fields.kind[schema_field_row] == lispb::codegen::PackedFieldKind::mini_float)};
        auto const width{field.has_value() && !representation_owned_width
                             ? effective_field_width(types, type, *field, variant)
                         : field.has_value() ? packed_fields.bit_width[schema_field_row]
                                             : packed_reserved_bits.bit_width[reserved_row]};
        auto const next_offset{checked_add(offset, width)};
        auto least_significant_bit{offset};
        if (packed_table.bit_order[packed_row] ==
            lispb::codegen::PackedBitOrder::most_significant_first) {
            least_significant_bit = 0;
            if (result.storage_bits.has_value() && next_offset.has_value() &&
                *next_offset <= *result.storage_bits) {
                least_significant_bit = *result.storage_bits - *next_offset;
            }
        }
        auto const field_row{fields.append()};
        fields.parent[field_row] = id;
        fields.segment[field_row] = segment;
        fields.name[field_row] = types.strings().view(
            field ? packed_fields.name[schema_field_row] : packed_reserved_bits.name[reserved_row]);
        fields.semantic_type[field_row] =
            field.has_value()
                ? std::optional<
                      lispb::schema::TypeId>{types.occurrences().type[types.occurrences().index(
                      packed_fields.semantic_type[schema_field_row])]}
                : std::nullopt;
        fields.logical_type[field_row] = context.storage.strings.intern(
            field.has_value() ? std::string{types.strings().view(types.cpp_spelling(
                                    types.occurrences().type[types.occurrences().index(
                                        packed_fields.semantic_type[schema_field_row])]))}
                              : "Reserved bits");
        fields.kind[field_row] = field.has_value()
                                   ? packed_fields.kind[schema_field_row]
                                   : lispb::codegen::PackedFieldKind::unsigned_integer;
        fields.reserved[field_row] = reserved.has_value();
        fields.schema_bit_width[field_row] = field.has_value()
                                               ? packed_fields.bit_width[schema_field_row]
                                               : packed_reserved_bits.bit_width[reserved_row];
        fields.schema_bit_width_auto[field_row] =
            field.has_value() && packed_fields.bit_width_auto[schema_field_row];
        fields.bit_width[field_row] = width;
        fields.overridden[field_row] = field_override && !representation_owned_width;
        fields.least_significant_bit[field_row] = least_significant_bit;
        fields.maximum_unsigned_value[field_row] =
            field.has_value() ? maximum_unsigned_value(width) : std::nullopt;
        fields.minimum_signed_value[field_row] =
            field.has_value() && packed_fields.kind[schema_field_row] ==
                                     lispb::codegen::PackedFieldKind::signed_integer
                ? minimum_signed_value(width)
                : std::nullopt;
        fields.maximum_signed_value[field_row] =
            field.has_value() && packed_fields.kind[schema_field_row] ==
                                     lispb::codegen::PackedFieldKind::signed_integer
                ? maximum_signed_value(width)
                : std::nullopt;
        fields.minimum_semantic_value[field_row] =
            field.has_value() ? packed_fields.minimum_value[schema_field_row] : std::nullopt;
        fields.maximum_semantic_value[field_row] =
            field.has_value() ? packed_fields.maximum_value[schema_field_row] : std::nullopt;

        if (field_override && representation_owned_width) {
            context.report(
                {DiagnosticSeverity::warning,
                 "Packed field '" +
                     std::string{types.strings().view(packed_fields.name[schema_field_row])} +
                     "' ignores a session width override because its " +
                     std::string{packed_fields.kind[schema_field_row] ==
                                         lispb::codegen::PackedFieldKind::linear_quantized
                                     ? "linear-quantized"
                                 : packed_fields.kind[schema_field_row] ==
                                         lispb::codegen::PackedFieldKind::fixed_point
                                     ? "fixed-point"
                                     : "mini-float"} +
                     " representation owns the exact encoded width."});
        }
        if (field.has_value() && !fields.maximum_unsigned_value[field_row].has_value()) {
            context.report(
                {DiagnosticSeverity::error,
                 "Packed field '" +
                     std::string{types.strings().view(packed_fields.name[schema_field_row])} +
                     "' exceeds the 64-bit V1 range limit."});
        }
        std::optional<lispb::codegen::PackedIntegerValue> minimum_required_code;
        std::optional<lispb::codegen::PackedIntegerValue> maximum_required_code;
        if (field.has_value()) {
            auto& codes{context.storage.codes};
            fields.codes[field_row].begin = codes.name.size();
            for (auto const code_id :
                 types.declarations().items(packed_fields.named_codes[schema_field_row])) {
                auto const code{code_id};
                auto const code_row{codes.append()};
                auto const named_row{types.declarations().packed_named_codes.index(code)};
                codes.name[code_row] =
                    types.strings().view(types.declarations().packed_named_codes.name[named_row]);
                codes.integer_value[code_row] =
                    types.declarations().packed_named_codes.value[named_row];
                codes.sentinel[code_row] =
                    types.declarations().packed_named_codes.sentinel[named_row];
                ++fields.codes[field_row].count;
                if (types.declarations()
                        .packed_named_codes
                        .sentinel[types.declarations().packed_named_codes.index(code)]) {
                    ++fields.sentinel_code_count[field_row];
                }
                if (!minimum_required_code.has_value() ||
                    lispb::codegen::packed_integer_less(
                        types.declarations()
                            .packed_named_codes
                            .value[types.declarations().packed_named_codes.index(code)],
                        *minimum_required_code)) {
                    minimum_required_code =
                        types.declarations()
                            .packed_named_codes
                            .value[types.declarations().packed_named_codes.index(code)];
                }
                if (!maximum_required_code.has_value() ||
                    lispb::codegen::packed_integer_less(
                        *maximum_required_code,
                        types.declarations()
                            .packed_named_codes
                            .value[types.declarations().packed_named_codes.index(code)])) {
                    maximum_required_code =
                        types.declarations()
                            .packed_named_codes
                            .value[types.declarations().packed_named_codes.index(code)];
                }
            }
        }
        if (fields.minimum_semantic_value[field_row].has_value() &&
            fields.maximum_semantic_value[field_row].has_value()) {
            auto const minimum{*fields.minimum_semantic_value[field_row]};
            auto const maximum{*fields.maximum_semantic_value[field_row]};
            fields.semantic_value_count[field_row] = packed_range_count(minimum, maximum);
            if (!minimum_required_code.has_value() ||
                lispb::codegen::packed_integer_less(minimum, *minimum_required_code)) {
                minimum_required_code = minimum;
            }
            if (!maximum_required_code.has_value() ||
                lispb::codegen::packed_integer_less(*maximum_required_code, maximum)) {
                maximum_required_code = maximum;
            }
            if (fields.semantic_value_count[field_row].has_value()) {
                fields.required_code_count[field_row] = checked_add(
                    *fields.semantic_value_count[field_row], fields.sentinel_code_count[field_row]);
            }
            auto const signed_domain{packed_fields.kind[schema_field_row] ==
                                     lispb::codegen::PackedFieldKind::signed_integer};
            auto const domain_fits{
                minimum_required_code.has_value() && maximum_required_code.has_value() &&
                (signed_domain
                     ? lispb::codegen::packed_integer_fits_signed(*minimum_required_code, width) &&
                           lispb::codegen::packed_integer_fits_signed(*maximum_required_code, width)
                     : lispb::codegen::packed_integer_fits_unsigned(*minimum_required_code,
                                                                    width) &&
                           lispb::codegen::packed_integer_fits_unsigned(*maximum_required_code,
                                                                        width))};
            if (!domain_fits) {
                context.report(
                    {DiagnosticSeverity::error,
                     "Packed field '" +
                         std::string{types.strings().view(packed_fields.name[schema_field_row])} +
                         "' semantic range or sentinel codes do not fit its planned width."});
            } else if (width == 64 && fields.required_code_count[field_row].has_value()) {
                fields.unused_codes[field_row] =
                    maximum_analysis_count - *fields.required_code_count[field_row] + 1;
            } else if (width == 64 && fields.sentinel_code_count[field_row] == 0) {
                auto const full_signed_domain{
                    signed_domain && minimum.negative &&
                    minimum.magnitude == (std::uint64_t{1} << 63) && !maximum.negative &&
                    maximum.magnitude == static_cast<std::uint64_t>(maximum_signed_code)};
                auto const full_unsigned_domain{!signed_domain && !minimum.negative &&
                                                minimum.magnitude == 0 && !maximum.negative &&
                                                maximum.magnitude == maximum_unsigned_code};
                if (full_signed_domain || full_unsigned_domain) {
                    fields.unused_codes[field_row] = 0;
                }
            } else if (fields.required_code_count[field_row].has_value()) {
                fields.unused_codes[field_row] =
                    (std::uint64_t{1} << width) - *fields.required_code_count[field_row];
            }
        }
        if (minimum_required_code.has_value() && maximum_required_code.has_value()) {
            fields.minimum_required_bits[field_row] = lispb::codegen::minimum_packed_integer_bits(
                *minimum_required_code,
                *maximum_required_code,
                field.has_value() && packed_fields.kind[schema_field_row] ==
                                         lispb::codegen::PackedFieldKind::signed_integer);
        }
        if (next_offset.has_value()) {
            auto const positioned{
                packed_table.bit_order[packed_row] ==
                    lispb::codegen::PackedBitOrder::least_significant_first ||
                (result.storage_bits.has_value() && *next_offset <= *result.storage_bits)};
            if (width != 0 && positioned) {
                fields.most_significant_bit[field_row] = least_significant_bit + width - 1;
            }
            offset = *next_offset;
        } else {
            width_overflow = true;
            context.report(
                {DiagnosticSeverity::error, "Packed segment bit offsets overflow uint64."});
        }
        auto const next_category_bits{
            checked_add(field.has_value() ? payload_bits : reserved_bits, width)};
        if (!next_category_bits.has_value()) {
            width_overflow = true;
        } else if (field.has_value()) {
            payload_bits = *next_category_bits;
        } else {
            reserved_bits = *next_category_bits;
        }
    }

    if (!width_overflow) {
        result.bits_used = offset;
        result.payload_bits = payload_bits;
        result.reserved_bits = reserved_bits;
        if (result.storage_bits.has_value()) {
            if (*result.bits_used > *result.storage_bits) {
                result.overflow_bits = *result.bits_used - *result.storage_bits;
                context.report(
                    {DiagnosticSeverity::error,
                     "Packed fields use more bits than the selected storage type provides."});
            } else {
                result.overflow_bits = 0;
                result.unused_bits = *result.storage_bits - *result.bits_used;
            }
        }
    }

    if (packed_table.invalid_raw_value[packed_row].has_value() && result.storage_bits.has_value() &&
        *result.storage_bits < 64) {
        auto const storage_max{
            maximum_unsigned_value(static_cast<std::uint32_t>(*result.storage_bits))};
        if (storage_max.has_value() && *packed_table.invalid_raw_value[packed_row] > *storage_max) {
            context.report(
                {DiagnosticSeverity::error,
                 "The invalid raw value does not fit the selected packed storage type."});
        }
    }

    result.fields.count = fields.parent.size() - result.fields.begin;
    context.storage.types.packed[id.value] = std::move(result);
}
}

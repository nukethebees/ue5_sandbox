#include "../access_intents.hpp"
#include "../analysis_limits.hpp"
#include "../checked_arithmetic.hpp"
#include "../integer_code_domain.hpp"
#include "../memory_regions.hpp"
#include "../relationship_extent.hpp"
#include "../variant_projection.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/access_intent.hpp>
#include <ioj/layout/analyzer/analyzer.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/packed_access_analysis.hpp>
#include <ioj/layout/analyzer/packed_access_comparison.hpp>
#include <ioj/layout/analyzer/packed_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_access_analysis.hpp>
#include <ioj/layout/analyzer/packed_field_analysis.hpp>
#include <ioj/layout/analyzer/packed_named_code_analysis.hpp>
#include <ioj/layout/analyzer/packed_target_comparison.hpp>
#include <ioj/layout/analyzer/physical_type_spelling.hpp>
#include <ioj/layout/analyzer/relationship_target_facts.hpp>
#include <ioj/layout/physical_facts.hpp>
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

namespace ioj::layout {
using namespace analysis_detail;

auto Analyzer::analyze_packed(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              ResolvedVariant const& variant,
                              AbiProfile const& abi,
                              std::uint64_t const element_count,
                              std::span<RelationshipTargetFacts const> const relationship_targets)
    -> PackedAnalysis {
    auto const& packed{types.definition(type)};
    auto const schema_storage{
        physical_type_spelling(
            types,
            types.definitions().packed.storage_type[types.definitions().packed.index(packed)])
            .value_or(std::string{
                types.cpp_types().spelling(types.occurrences().cpp_type[types.occurrences().index(
                    types.definitions()
                        .packed.storage_type[types.definitions().packed.index(packed)])])})};
    PackedAnalysis result{
        .type = type,
        .schema_storage_type = schema_storage,
        .storage_type = effective_storage_type(types, type, packed, variant),
        .storage_overridden = variant.overrides.packed_storage_types.contains(type),
        .byte_order =
            types.definitions().packed.byte_order[types.definitions().packed.index(packed)],
        .bit_order = types.definitions().packed.bit_order[types.definitions().packed.index(packed)],
        .invalid_raw_value =
            types.definitions().packed.invalid_raw_value[types.definitions().packed.index(packed)],
        .aggregate = {.element_count = element_count}};
    auto physical{PhysicalFactsResolver{types, abi, &variant}.resolve(type)};
    result.storage_facts = physical.facts;
    result.diagnostics = std::move(physical.diagnostics);
    if (!result.storage_facts.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Unknown physical facts for packed storage type '" + result.storage_type + "'."});
    } else {
        result.storage_bits = checked_multiply(result.storage_facts->size_bytes, 8);
        if (!result.storage_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed storage bit count overflows uint64."});
        }
        if (!result.storage_facts->unsigned_value_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Packed storage type '" + result.storage_type +
                     "' is not described as an unsigned integer by this ABI profile."});
        }
    }

    std::uint64_t offset{};
    std::uint64_t payload_bits{};
    std::uint64_t reserved_bits{};
    bool width_overflow{};
    result.fields.reserve(types.definitions().packed.segments_of(packed).size());
    for (auto const& segment : types.definitions().packed.segments_of(packed)) {
        auto const field{((segment).kind() == lispb::schema::PackedSegmentKind::field
                              ? std::optional{segment}
                              : std::nullopt)};
        auto const reserved{((segment).kind() == lispb::schema::PackedSegmentKind::reserved
                                 ? std::optional{segment}
                                 : std::nullopt)};
        auto const field_override{
            field.has_value() &&
            variant.overrides.packed_field_widths.contains(FieldOverrideId{
                .type = type,
                .field_name =
                    types.definitions()
                        .packed_fields.name[types.definitions().packed_fields.index(*field)]})};
        auto const representation_owned_width{
            field.has_value() &&
            (types.definitions()
                     .packed_fields.kind[types.definitions().packed_fields.index(*field)] ==
                 lispb::codegen::PackedFieldKind::linear_quantized ||
             types.definitions()
                     .packed_fields.kind[types.definitions().packed_fields.index(*field)] ==
                 lispb::codegen::PackedFieldKind::fixed_point ||
             types.definitions()
                     .packed_fields.kind[types.definitions().packed_fields.index(*field)] ==
                 lispb::codegen::PackedFieldKind::mini_float)};
        auto const width{
            field.has_value() && !representation_owned_width
                ? effective_field_width(types, type, *field, variant)
            : field.has_value()
                ? types.definitions()
                      .packed_fields.bit_width[types.definitions().packed_fields.index(*field)]
                : types.definitions()
                      .packed_reserved_bits
                      .bit_width[types.definitions().packed_reserved_bits.index(*reserved)]};
        auto const next_offset{checked_add(offset, width)};
        auto least_significant_bit{offset};
        if (types.definitions().packed.bit_order[types.definitions().packed.index(packed)] ==
            lispb::codegen::PackedBitOrder::most_significant_first) {
            least_significant_bit = 0;
            if (result.storage_bits.has_value() && next_offset.has_value() &&
                *next_offset <= *result.storage_bits) {
                least_significant_bit = *result.storage_bits - *next_offset;
            }
        }
        PackedFieldAnalysis field_result{
            .name =
                field.has_value()
                    ? std::string{types.strings().view(
                          types.definitions()
                              .packed_fields.name[types.definitions().packed_fields.index(*field)])}
                    : std::string{types.strings().view(
                          types.definitions()
                              .packed_reserved_bits
                              .name[types.definitions().packed_reserved_bits.index(*reserved)])},
            .semantic_type =
                field.has_value()
                    ? std::optional<
                          lispb::schema::TypeId>{types.occurrences().type[types.occurrences().index(
                          types.definitions().packed_fields.semantic_type
                              [types.definitions().packed_fields.index(*field)])]}
                    : std::nullopt,
            .logical_type = field.has_value()
                              ? std::string{types.strings().view(types.cpp_spelling(
                                    types.occurrences().type[types.occurrences().index(
                                        types.definitions().packed_fields.semantic_type
                                            [types.definitions().packed_fields.index(*field)])]))}
                              : "Reserved bits",
            .kind = field.has_value()
                      ? types.definitions()
                            .packed_fields.kind[types.definitions().packed_fields.index(*field)]
                      : lispb::codegen::PackedFieldKind::unsigned_integer,
            .reserved = reserved.has_value(),
            .schema_bit_width =
                field.has_value()
                    ? types.definitions()
                          .packed_fields.bit_width[types.definitions().packed_fields.index(*field)]
                    : types.definitions()
                          .packed_reserved_bits
                          .bit_width[types.definitions().packed_reserved_bits.index(*reserved)],
            .schema_bit_width_auto =
                field.has_value() &&
                types.definitions()
                    .packed_fields.bit_width_auto[types.definitions().packed_fields.index(*field)],
            .bit_width = width,
            .overridden = field_override && !representation_owned_width,
            .least_significant_bit = least_significant_bit,
            .maximum_unsigned_value =
                field.has_value() ? maximum_unsigned_value(width) : std::nullopt,
            .minimum_signed_value =
                field.has_value() &&
                        types.definitions()
                                .packed_fields
                                .kind[types.definitions().packed_fields.index(*field)] ==
                            lispb::codegen::PackedFieldKind::signed_integer
                    ? minimum_signed_value(width)
                    : std::nullopt,
            .maximum_signed_value =
                field.has_value() &&
                        types.definitions()
                                .packed_fields
                                .kind[types.definitions().packed_fields.index(*field)] ==
                            lispb::codegen::PackedFieldKind::signed_integer
                    ? maximum_signed_value(width)
                    : std::nullopt,
            .minimum_semantic_value =
                field.has_value()
                    ? types.definitions()
                          .packed_fields
                          .minimum_value[types.definitions().packed_fields.index(*field)]
                    : std::nullopt,
            .maximum_semantic_value =
                field.has_value()
                    ? types.definitions()
                          .packed_fields
                          .maximum_value[types.definitions().packed_fields.index(*field)]
                    : std::nullopt,
            .relationship_kind =
                field.has_value() &&
                        lispb::schema::is_valid(
                            types.definitions()
                                .packed_fields
                                .relationship[types.definitions().packed_fields.index(*field)])
                    ? std::optional{types.relationships().kind[types.relationships().index(
                          types.definitions().packed_fields.relationship
                              [types.definitions().packed_fields.index(*field)])]}
                    : std::nullopt,
            .relationship_unit =
                field.has_value() &&
                        lispb::schema::is_valid(
                            types.definitions()
                                .packed_fields
                                .relationship[types.definitions().packed_fields.index(*field)])
                    ? types.relationships().unit[types.relationships().index(
                          types.definitions().packed_fields.relationship
                              [types.definitions().packed_fields.index(*field)])]
                    : std::nullopt,
            .relationship_target =
                field.has_value() &&
                        lispb::schema::is_valid(
                            types.definitions()
                                .packed_fields
                                .relationship[types.definitions().packed_fields.index(*field)])
                    ? std::optional{std::string{types.strings().view(
                          types.name(types.occurrences().type[types.occurrences().index(
                              types.relationships().target[types.relationships().index(
                                  types.definitions().packed_fields.relationship
                                      [types.definitions().packed_fields.index(*field)])])]))}}
                    : std::nullopt};
        if (width == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed segment '" + field_result.name + "' must use at least one bit."});
        }
        if (field_override && representation_owned_width) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Packed field '" +
                     std::string{types.strings().view(
                         types.definitions()
                             .packed_fields
                             .name[types.definitions().packed_fields.index(*field)])} +
                     "' ignores a session width override because its " +
                     std::string{
                         types.definitions().packed_fields.kind[types.definitions()
                                                                    .packed_fields.index(*field)] ==
                                 lispb::codegen::PackedFieldKind::linear_quantized
                             ? "linear-quantized"
                         : types.definitions()
                                     .packed_fields
                                     .kind[types.definitions().packed_fields.index(*field)] ==
                                 lispb::codegen::PackedFieldKind::fixed_point
                             ? "fixed-point"
                             : "mini-float"} +
                     " representation owns the exact encoded width."});
        }
        if (field.has_value() && !field_result.maximum_unsigned_value.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" +
                     std::string{types.strings().view(
                         types.definitions()
                             .packed_fields
                             .name[types.definitions().packed_fields.index(*field)])} +
                     "' exceeds the 64-bit V1 range limit."});
        }
        std::optional<lispb::codegen::PackedIntegerValue> minimum_required_code;
        std::optional<lispb::codegen::PackedIntegerValue> maximum_required_code;
        if (field.has_value()) {
            if (((types.definition(types.occurrences().type[types.occurrences().index(
                      types.definitions().packed_fields.semantic_type
                          [types.definitions().packed_fields.index(*field)])]))
                     .kind() == lispb::schema::TypeDefinitionKind::linear_quantized)) {
                field_result.linear_quantized = analyze_linear_quantized(
                    types,
                    types.occurrences().type[types.occurrences().index(
                        types.definitions().packed_fields.semantic_type
                            [types.definitions().packed_fields.index(*field)])]);
            } else if (((types.definition(types.occurrences().type[types.occurrences().index(
                             types.definitions().packed_fields.semantic_type
                                 [types.definitions().packed_fields.index(*field)])]))
                            .kind() == lispb::schema::TypeDefinitionKind::fixed_point)) {
                field_result.fixed_point = analyze_fixed_point(
                    types,
                    types.occurrences().type[types.occurrences().index(
                        types.definitions().packed_fields.semantic_type
                            [types.definitions().packed_fields.index(*field)])]);
            } else if (((types.definition(types.occurrences().type[types.occurrences().index(
                             types.definitions().packed_fields.semantic_type
                                 [types.definitions().packed_fields.index(*field)])]))
                            .kind() == lispb::schema::TypeDefinitionKind::mini_float)) {
                field_result.mini_float =
                    analyze_mini_float(types,
                                       types.occurrences().type[types.occurrences().index(
                                           types.definitions().packed_fields.semantic_type
                                               [types.definitions().packed_fields.index(*field)])]);
            }
            field_result.named_codes.reserve(
                types.definitions()
                    .packed_fields.named_codes[types.definitions().packed_fields.index(*field)]
                    .size());
            for (auto const code_id : types.declarations().items(
                     types.definitions()
                         .packed_fields
                         .named_codes[types.definitions().packed_fields.index(*field)])) {
                auto const code{code_id};
                field_result.named_codes.push_back(
                    {.name = std::string{types.strings().view(
                         types.declarations()
                             .packed_named_codes
                             .name[types.declarations().packed_named_codes.index(code)])},
                     .value = types.declarations()
                                  .packed_named_codes
                                  .value[types.declarations().packed_named_codes.index(code)],
                     .sentinel =
                         types.declarations()
                             .packed_named_codes
                             .sentinel[types.declarations().packed_named_codes.index(code)] != 0});
                if (types.declarations()
                        .packed_named_codes
                        .sentinel[types.declarations().packed_named_codes.index(code)]) {
                    ++field_result.sentinel_code_count;
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
        if (field_result.minimum_semantic_value.has_value() &&
            field_result.maximum_semantic_value.has_value()) {
            auto const minimum{*field_result.minimum_semantic_value};
            auto const maximum{*field_result.maximum_semantic_value};
            field_result.semantic_value_count = packed_range_count(minimum, maximum);
            if (!minimum_required_code.has_value() ||
                lispb::codegen::packed_integer_less(minimum, *minimum_required_code)) {
                minimum_required_code = minimum;
            }
            if (!maximum_required_code.has_value() ||
                lispb::codegen::packed_integer_less(*maximum_required_code, maximum)) {
                maximum_required_code = maximum;
            }
            if (field_result.semantic_value_count.has_value()) {
                field_result.required_code_count = checked_add(*field_result.semantic_value_count,
                                                               field_result.sentinel_code_count);
            }
            auto const signed_domain{
                types.definitions()
                    .packed_fields.kind[types.definitions().packed_fields.index(*field)] ==
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
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed field '" +
                         std::string{types.strings().view(
                             types.definitions()
                                 .packed_fields
                                 .name[types.definitions().packed_fields.index(*field)])} +
                         "' semantic range or sentinel codes do not fit its planned width."});
            } else if (width == 64 && field_result.required_code_count.has_value()) {
                field_result.unused_codes =
                    maximum_analysis_count - *field_result.required_code_count + 1;
            } else if (width == 64 && field_result.sentinel_code_count == 0) {
                auto const full_signed_domain{
                    signed_domain && minimum.negative &&
                    minimum.magnitude == (std::uint64_t{1} << 63) && !maximum.negative &&
                    maximum.magnitude == static_cast<std::uint64_t>(maximum_signed_code)};
                auto const full_unsigned_domain{!signed_domain && !minimum.negative &&
                                                minimum.magnitude == 0 && !maximum.negative &&
                                                maximum.magnitude == maximum_unsigned_code};
                if (full_signed_domain || full_unsigned_domain) {
                    field_result.unused_codes = 0;
                }
            } else if (field_result.required_code_count.has_value()) {
                field_result.unused_codes =
                    (std::uint64_t{1} << width) - *field_result.required_code_count;
            }
        }
        if (minimum_required_code.has_value() && maximum_required_code.has_value()) {
            field_result.minimum_required_bits = lispb::codegen::minimum_packed_integer_bits(
                *minimum_required_code,
                *maximum_required_code,
                field.has_value() &&
                    types.definitions()
                            .packed_fields.kind[types.definitions().packed_fields.index(*field)] ==
                        lispb::codegen::PackedFieldKind::signed_integer);
        }
        if (field.has_value() &&
            lispb::schema::is_valid(
                types.definitions()
                    .packed_fields.relationship[types.definitions().packed_fields.index(*field)]) &&
            (types.relationships().kind[types.relationships().index(
                 types.definitions().packed_fields.relationship
                     [types.definitions().packed_fields.index(*field)])] ==
                 lispb::codegen::SemanticRelationKind::index_into ||
             types.relationships().kind[types.relationships().index(
                 types.definitions().packed_fields.relationship
                     [types.definitions().packed_fields.index(*field)])] ==
                 lispb::codegen::SemanticRelationKind::count_of ||
             types.relationships().kind[types.relationships().index(
                 types.definitions().packed_fields.relationship
                     [types.definitions().packed_fields.index(*field)])] ==
                 lispb::codegen::SemanticRelationKind::offset_into)) {
            auto const target_facts{
                std::ranges::find(relationship_targets,
                                  types.occurrences().type[types.occurrences().index(
                                      types.relationships().target[types.relationships().index(
                                          types.definitions().packed_fields.relationship
                                              [types.definitions().packed_fields.index(*field)])])],
                                  &RelationshipTargetFacts::target)};
            if (target_facts != relationship_targets.end()) {
                auto const target_extent{relationship_target_extent(
                    types,
                    types.definitions()
                        .packed_fields
                        .relationship[types.definitions().packed_fields.index(*field)],
                    *target_facts)};
                if (target_extent.has_value()) {
                    auto const extent_analysis{analyze_unsigned_relationship_extent(
                        types.declarations(),
                        "Packed field '" +
                            std::string{types.strings().view(
                                types.definitions()
                                    .packed_fields
                                    .name[types.definitions().packed_fields.index(*field)])} +
                            "'",
                        types.relationships().kind[types.relationships().index(
                            types.definitions().packed_fields.relationship
                                [types.definitions().packed_fields.index(*field)])],
                        *target_extent,
                        types.definitions()
                            .packed_fields
                            .named_codes[types.definitions().packed_fields.index(*field)],
                        width,
                        types.definitions()
                            .packed_fields
                            .minimum_value[types.definitions().packed_fields.index(*field)],
                        types.definitions()
                            .packed_fields
                            .maximum_value[types.definitions().packed_fields.index(*field)])};
                    field_result.relationship_target_extent = extent_analysis.target_extent;
                    field_result.relationship_live_value_count = extent_analysis.live_values;
                    field_result.relationship_required_code_count = extent_analysis.required_codes;
                    field_result.relationship_minimum_required_bits =
                        extent_analysis.minimum_required_bits;
                    field_result.relationship_width_sufficient = extent_analysis.width_sufficient;
                    field_result.relationship_code_space_capacity_limit =
                        extent_analysis.code_space_capacity_limit;
                    field_result.relationship_capacity_headroom = extent_analysis.capacity_headroom;
                    field_result.relationship_semantic_capacity_limit =
                        extent_analysis.semantic_capacity_limit;
                    field_result.relationship_sentinel_capacity_limit =
                        extent_analysis.sentinel_capacity_limit;
                    field_result.relationship_effective_capacity_limit =
                        extent_analysis.effective_capacity_limit;
                    field_result.relationship_effective_capacity_headroom =
                        extent_analysis.effective_capacity_headroom;
                    result.diagnostics.insert(result.diagnostics.end(),
                                              extent_analysis.diagnostics.begin(),
                                              extent_analysis.diagnostics.end());
                }
            }
        }
        if (next_offset.has_value()) {
            auto const positioned{
                types.definitions().packed.bit_order[types.definitions().packed.index(packed)] ==
                    lispb::codegen::PackedBitOrder::least_significant_first ||
                (result.storage_bits.has_value() && *next_offset <= *result.storage_bits)};
            if (width != 0 && positioned) {
                field_result.most_significant_bit = least_significant_bit + width - 1;
            }
            offset = *next_offset;
        } else {
            width_overflow = true;
            result.diagnostics.push_back(
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
        result.fields.push_back(std::move(field_result));
    }

    if (!width_overflow) {
        result.bits_used = offset;
        result.payload_bits = payload_bits;
        result.reserved_bits = reserved_bits;
        if (result.storage_bits.has_value()) {
            if (*result.bits_used > *result.storage_bits) {
                result.overflow_bits = *result.bits_used - *result.storage_bits;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed fields use more bits than the selected storage type provides."});
            } else {
                result.unused_bits = *result.storage_bits - *result.bits_used;
            }
        }
    }

    if (types.definitions()
            .packed.invalid_raw_value[types.definitions().packed.index(packed)]
            .has_value() &&
        result.storage_bits.has_value() && *result.storage_bits < 64) {
        auto const storage_max{
            maximum_unsigned_value(static_cast<std::uint32_t>(*result.storage_bits))};
        if (storage_max.has_value() &&
            *types.definitions()
                    .packed.invalid_raw_value[types.definitions().packed.index(packed)] >
                *storage_max) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "The invalid raw value does not fit the selected packed storage type."});
        }
    }

    if (result.storage_facts.has_value()) {
        auto const element_bytes{result.storage_facts->size_bytes};
        if (element_bytes == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed storage type has a zero-byte physical size."});
        } else {
            result.aggregate.total_storage_bytes = checked_multiply(element_bytes, element_count);
            if (!result.aggregate.total_storage_bytes.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Packed aggregate byte count overflows uint64."});
            }

            auto const& memory{abi.memory_facts()};
            result.aggregate.cache_line_bytes = memory.cache_line_bytes;
            if (!memory.cache_line_bytes.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
            } else if (*memory.cache_line_bytes == 0) {
                result.aggregate.cache_line_bytes.reset();
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
            } else {
                if (result.aggregate.total_storage_bytes.has_value()) {
                    result.aggregate.minimum_cache_lines = minimum_regions(
                        *result.aggregate.total_storage_bytes, *memory.cache_line_bytes);
                }
                result.aggregate.complete_elements_per_cache_line =
                    *memory.cache_line_bytes / element_bytes;
                result.aggregate.cache_line_straddling_elements =
                    straddling_elements(element_bytes,
                                        element_count,
                                        *memory.cache_line_bytes,
                                        result.aggregate.total_storage_bytes);
            }

            result.aggregate.page_bytes = memory.page_bytes;
            if (!memory.page_bytes.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Page size is unknown for ABI profile '" + abi.name() + "'."});
            } else if (*memory.page_bytes == 0) {
                result.aggregate.page_bytes.reset();
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
            } else {
                if (result.aggregate.total_storage_bytes.has_value()) {
                    result.aggregate.minimum_pages =
                        minimum_regions(*result.aggregate.total_storage_bytes, *memory.page_bytes);
                }
                result.aggregate.complete_elements_per_page = *memory.page_bytes / element_bytes;
                result.aggregate.page_straddling_elements =
                    straddling_elements(element_bytes,
                                        element_count,
                                        *memory.page_bytes,
                                        result.aggregate.total_storage_bytes);
            }
        }
    }
    if (result.payload_bits.has_value()) {
        result.aggregate.total_payload_bits = checked_multiply(*result.payload_bits, element_count);
        if (!result.aggregate.total_payload_bits.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Packed aggregate payload bit count overflows uint64."});
        }
    }
    if (result.reserved_bits.has_value()) {
        result.aggregate.total_reserved_bits =
            checked_multiply(*result.reserved_bits, element_count);
        if (!result.aggregate.total_reserved_bits.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Packed aggregate reserved bit count overflows uint64."});
        }
    }
    if (result.unused_bits.has_value()) {
        result.aggregate.total_unused_bits = checked_multiply(*result.unused_bits, element_count);
        if (!result.aggregate.total_unused_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed aggregate unused bit count overflows uint64."});
        }
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());
    return result;
}

auto Analyzer::compare_packed_targets(PackedAnalysis const& first, PackedAnalysis const& second)
    -> PackedTargetComparison {
    PackedTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target packed layout: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target packed layout: " + diagnostic.message});
    }

    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target packed analyses describe different semantic types."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target packed analyses use different element counts."});
        return result;
    }
    if (first.schema_storage_type != second.schema_storage_type ||
        first.storage_type != second.storage_type ||
        first.storage_overridden != second.storage_overridden ||
        first.byte_order != second.byte_order || first.bit_order != second.bit_order ||
        first.invalid_raw_value != second.invalid_raw_value ||
        first.fields.size() != second.fields.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target packed analyses do not use the same physical variant."});
        return result;
    }

    auto named_codes_match = [](std::span<PackedNamedCodeAnalysis const> const first_codes,
                                std::span<PackedNamedCodeAnalysis const> const second_codes) {
        if (first_codes.size() != second_codes.size()) {
            return false;
        }
        for (std::size_t index{}; index < first_codes.size(); ++index) {
            if (first_codes[index].name != second_codes[index].name ||
                first_codes[index].value != second_codes[index].value ||
                first_codes[index].sentinel != second_codes[index].sentinel) {
                return false;
            }
        }
        return true;
    };
    StringSet names;
    for (std::size_t index{}; index < first.fields.size(); ++index) {
        auto const& first_field{first.fields[index]};
        auto const& second_field{second.fields[index]};
        auto const unique_name{names.insert(first_field.name).second};
        if (!unique_name || first_field.name != second_field.name ||
            first_field.semantic_type != second_field.semantic_type ||
            first_field.logical_type != second_field.logical_type ||
            first_field.kind != second_field.kind ||
            first_field.reserved != second_field.reserved ||
            first_field.schema_bit_width != second_field.schema_bit_width ||
            first_field.schema_bit_width_auto != second_field.schema_bit_width_auto ||
            first_field.bit_width != second_field.bit_width ||
            first_field.overridden != second_field.overridden ||
            first_field.minimum_semantic_value != second_field.minimum_semantic_value ||
            first_field.maximum_semantic_value != second_field.maximum_semantic_value ||
            first_field.sentinel_code_count != second_field.sentinel_code_count ||
            !named_codes_match(first_field.named_codes, second_field.named_codes) ||
            first_field.relationship_kind != second_field.relationship_kind ||
            first_field.relationship_unit != second_field.relationship_unit ||
            first_field.relationship_target != second_field.relationship_target) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target packed analyses do not contain the same ordered segments and "
                 "physical variant."});
            return result;
        }
    }

    result.fields.reserve(first.fields.size());
    for (std::size_t index{}; index < first.fields.size(); ++index) {
        auto const& first_field{first.fields[index]};
        auto const& second_field{second.fields[index]};
        result.fields.push_back(
            {.name = first_field.name,
             .first = first_field,
             .second = second_field,
             .least_significant_bit_delta = numeric_delta(first_field.least_significant_bit,
                                                          second_field.least_significant_bit),
             .most_significant_bit_delta =
                 numeric_delta(first_field.most_significant_bit, second_field.most_significant_bit),
             .unused_code_delta =
                 numeric_delta(first_field.unused_codes, second_field.unused_codes),
             .relationship_target_extent_delta = numeric_delta(
                 first_field.relationship_target_extent, second_field.relationship_target_extent),
             .relationship_minimum_required_bit_delta =
                 numeric_delta(first_field.relationship_minimum_required_bits,
                               second_field.relationship_minimum_required_bits),
             .relationship_capacity_headroom_delta =
                 numeric_delta(first_field.relationship_capacity_headroom,
                               second_field.relationship_capacity_headroom),
             .relationship_effective_capacity_limit_delta =
                 numeric_delta(first_field.relationship_effective_capacity_limit,
                               second_field.relationship_effective_capacity_limit),
             .relationship_effective_capacity_headroom_delta =
                 numeric_delta(first_field.relationship_effective_capacity_headroom,
                               second_field.relationship_effective_capacity_headroom)});
    }

    auto const first_storage_size{
        first.storage_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const second_storage_size{
        second.storage_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const first_storage_alignment{first.storage_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const second_storage_alignment{second.storage_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const first_storage_value_bits{first.storage_facts.and_then(
        [](TypeFacts const& facts) { return facts.unsigned_value_bits; })};
    auto const second_storage_value_bits{second.storage_facts.and_then(
        [](TypeFacts const& facts) { return facts.unsigned_value_bits; })};
    result.storage_size_delta = numeric_delta(first_storage_size, second_storage_size);
    result.storage_alignment_delta =
        numeric_delta(first_storage_alignment, second_storage_alignment);
    result.storage_value_bit_delta =
        numeric_delta(first_storage_value_bits, second_storage_value_bits);
    result.storage_bit_delta = numeric_delta(first.storage_bits, second.storage_bits);
    result.bits_used_delta = numeric_delta(first.bits_used, second.bits_used);
    result.payload_bit_delta = numeric_delta(first.payload_bits, second.payload_bits);
    result.reserved_bit_delta = numeric_delta(first.reserved_bits, second.reserved_bits);
    result.unused_bit_delta = numeric_delta(first.unused_bits, second.unused_bits);
    auto known_overflow_bits = [](PackedAnalysis const& analysis) -> std::optional<std::uint64_t> {
        if (!analysis.storage_bits.has_value() || !analysis.bits_used.has_value()) {
            return std::nullopt;
        }
        return analysis.overflow_bits.value_or(0);
    };
    result.overflow_bit_delta =
        numeric_delta(known_overflow_bits(first), known_overflow_bits(second));
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_payload_bit_delta =
        numeric_delta(first.aggregate.total_payload_bits, second.aggregate.total_payload_bits);
    result.total_reserved_bit_delta =
        numeric_delta(first.aggregate.total_reserved_bits, second.aggregate.total_reserved_bits);
    result.total_unused_bit_delta =
        numeric_delta(first.aggregate.total_unused_bits, second.aggregate.total_unused_bits);
    result.cache_line_size_delta =
        numeric_delta(first.aggregate.cache_line_bytes, second.aggregate.cache_line_bytes);
    result.minimum_cache_line_delta =
        numeric_delta(first.aggregate.minimum_cache_lines, second.aggregate.minimum_cache_lines);
    result.complete_elements_per_cache_line_delta =
        numeric_delta(first.aggregate.complete_elements_per_cache_line,
                      second.aggregate.complete_elements_per_cache_line);
    result.cache_line_straddling_delta =
        numeric_delta(first.aggregate.cache_line_straddling_elements,
                      second.aggregate.cache_line_straddling_elements);
    result.page_size_delta = numeric_delta(first.aggregate.page_bytes, second.aggregate.page_bytes);
    result.minimum_page_delta =
        numeric_delta(first.aggregate.minimum_pages, second.aggregate.minimum_pages);
    result.complete_elements_per_page_delta = numeric_delta(
        first.aggregate.complete_elements_per_page, second.aggregate.complete_elements_per_page);
    result.page_straddling_delta = numeric_delta(first.aggregate.page_straddling_elements,
                                                 second.aggregate.page_straddling_elements);
    return result;
}

auto Analyzer::analyze_packed_access(PackedAnalysis const& packed,
                                     std::span<AccessIntent const> const accesses,
                                     AbiProfile const& abi,
                                     std::uint64_t const multiplicity) -> PackedAccessAnalysis {
    PackedAccessAnalysis result;
    result.type = packed.type;
    result.element_count = packed.aggregate.element_count;
    result.multiplicity = multiplicity;
    result.storage_footprint_bytes = packed.aggregate.total_storage_bytes;
    result.cache_line_bytes = packed.aggregate.cache_line_bytes;
    result.minimum_cache_lines_touched = packed.aggregate.minimum_cache_lines;
    result.page_bytes = packed.aggregate.page_bytes;
    result.minimum_pages_touched = packed.aggregate.minimum_pages;

    if (multiplicity == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Access multiplicity must be non-zero."});
    }

    if (result.storage_footprint_bytes.has_value()) {
        result.storage_footprint_bits = checked_multiply(*result.storage_footprint_bytes, 8);
        if (!result.storage_footprint_bits.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Packed storage footprint bit count overflows uint64."});
        }
    } else {
        result.diagnostics.push_back({packed.storage_facts.has_value()
                                          ? DiagnosticSeverity::error
                                          : DiagnosticSeverity::warning,
                                      "Packed storage footprint is Unknown."});
    }

    StringSet unique_names;
    std::uint64_t useful_bits_per_element{};
    std::uint64_t read_bits_per_element{};
    std::uint64_t write_bits_per_element{};
    bool read_selected{};
    bool write_selected{};
    for (auto const& access : accesses) {
        if (!accept_unique_access(
                unique_names, result.accesses, result.diagnostics, access, "packed field")) {
            continue;
        }

        auto const field{std::ranges::find(packed.fields, access.name, &PackedFieldAnalysis::name)};
        if (field == packed.fields.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected packed field '" + access.name + "' no longer exists."});
            continue;
        }
        if (field->reserved) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Reserved packed region '" + access.name + "' cannot be selected for access."});
            continue;
        }

        auto accumulate_bits = [&](std::uint64_t& total, char const* const category) -> bool {
            auto const next{checked_add(total, field->bit_width)};
            if (!next.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              std::string{"Selected packed-field "} + category +
                                                  " bit count per element overflows uint64."});
                return false;
            }
            total = *next;
            return true;
        };
        if (!accumulate_bits(useful_bits_per_element, "useful")) {
            return result;
        }
        if (access_includes_read(access.operation)) {
            read_selected = true;
            if (!accumulate_bits(read_bits_per_element, "read")) {
                return result;
            }
        }
        if (access_includes_write(access.operation)) {
            write_selected = true;
            if (!accumulate_bits(write_bits_per_element, "write")) {
                return result;
            }
        }

        PackedFieldAccessAnalysis field_access;
        field_access.name = access.name;
        field_access.operation = access.operation;
        field_access.bit_width = field->bit_width;
        field_access.useful_bits = checked_multiply(field->bit_width, result.element_count);
        if (!field_access.useful_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected packed field '" + access.name + "' useful bit count overflows uint64."});
        }
        field_access.read_useful_bits = access_includes_read(access.operation)
                                          ? field_access.useful_bits
                                          : std::optional<std::uint64_t>{0};
        field_access.write_useful_bits = access_includes_write(access.operation)
                                           ? field_access.useful_bits
                                           : std::optional<std::uint64_t>{0};
        if (multiplicity != 0) {
            field_access.logical_read_useful_bits = field_access.read_useful_bits.and_then(
                [&](std::uint64_t const bits) { return checked_multiply(bits, multiplicity); });
            field_access.logical_write_useful_bits = field_access.write_useful_bits.and_then(
                [&](std::uint64_t const bits) { return checked_multiply(bits, multiplicity); });
            if (field_access.read_useful_bits.has_value() &&
                !field_access.logical_read_useful_bits.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected packed field '" + access.name +
                         "' logical read useful bit count overflows uint64."});
            }
            if (field_access.write_useful_bits.has_value() &&
                !field_access.logical_write_useful_bits.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected packed field '" + access.name +
                         "' logical write useful bit count overflows uint64."});
            }
        }
        result.field_names.push_back(access.name);
        result.accesses.push_back(access);
        result.fields.push_back(std::move(field_access));
    }

    if (result.accesses.empty()) {
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No packed fields are selected for access."});
        }
        result.cache_footprint_capacity = cache_capacity_analysis(std::nullopt, abi.memory_facts());
        return result;
    }

    auto scale_bits = [&](std::uint64_t const per_element,
                          char const* const category) -> std::optional<std::uint64_t> {
        auto const total{checked_multiply(per_element, result.element_count)};
        if (!total.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          std::string{"Selected packed-field "} + category +
                                              " bit count overflows uint64."});
        }
        return total;
    };
    result.useful_bits = scale_bits(useful_bits_per_element, "useful");
    result.read_useful_bits = scale_bits(read_bits_per_element, "read useful");
    result.write_useful_bits = scale_bits(write_bits_per_element, "write useful");
    if (multiplicity != 0) {
        result.logical_read_useful_bits = result.read_useful_bits.and_then(
            [&](std::uint64_t const bits) { return checked_multiply(bits, multiplicity); });
        result.logical_write_useful_bits = result.write_useful_bits.and_then(
            [&](std::uint64_t const bits) { return checked_multiply(bits, multiplicity); });
        if (result.read_useful_bits.has_value() && !result.logical_read_useful_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Logical read useful bit total overflows uint64."});
        }
        if (result.write_useful_bits.has_value() && !result.logical_write_useful_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Logical write useful bit total overflows uint64."});
        }
    }

    if (result.storage_footprint_bits.has_value() && result.useful_bits.has_value()) {
        if (*result.useful_bits <= *result.storage_footprint_bits) {
            result.non_useful_storage_bits = *result.storage_footprint_bits - *result.useful_bits;
        } else {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected useful bits exceed the packed physical storage footprint."});
        }
    }
    if (packed.unused_bits.has_value() && result.storage_footprint_bits.has_value()) {
        std::uint64_t unselected_bits_per_element{};
        bool unselected_bits_known{true};
        for (auto const& field : packed.fields) {
            if (field.reserved ||
                std::ranges::find(result.field_names, field.name) != result.field_names.end()) {
                continue;
            }
            auto const next{checked_add(unselected_bits_per_element, field.bit_width)};
            if (!next.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Unselected packed-field bit count per element overflows uint64."});
                unselected_bits_known = false;
                break;
            }
            unselected_bits_per_element = *next;
        }
        auto scale_category = [&](std::optional<std::uint64_t> const per_element,
                                  char const* const category) -> std::optional<std::uint64_t> {
            if (!per_element.has_value()) {
                return std::nullopt;
            }
            auto const scaled{checked_multiply(*per_element, result.element_count)};
            if (!scaled.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     std::string{"Packed access "} + category + " bit count overflows uint64."});
            }
            return scaled;
        };
        result.unselected_field_bits = scale_category(
            unselected_bits_known ? std::optional{unselected_bits_per_element} : std::nullopt,
            "unselected-field");
        result.reserved_region_bits = scale_category(packed.reserved_bits, "reserved-region");
        result.physically_unused_storage_bits =
            scale_category(packed.unused_bits, "physically-unused-storage");

        if (result.unselected_field_bits.has_value() && result.reserved_region_bits.has_value() &&
            result.physically_unused_storage_bits.has_value() &&
            result.non_useful_storage_bits.has_value()) {
            auto const categorized{
                checked_add(*result.unselected_field_bits, *result.reserved_region_bits)
                    .and_then([&](std::uint64_t const bits) {
                        return checked_add(bits, *result.physically_unused_storage_bits);
                    })};
            if (!categorized.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed non-useful storage category total overflows uint64."});
            } else if (*categorized != *result.non_useful_storage_bits) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed non-useful storage categories do not match the total."});
            }
        }
    } else if (packed.overflow_bits.has_value() && *packed.overflow_bits != 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Packed non-useful storage categories are Unknown because fields exceed storage."});
    }

    auto derive_region_bytes = [&](std::optional<std::uint64_t> const regions,
                                   std::optional<std::uint64_t> const bytes_per_region,
                                   std::optional<std::uint64_t>& output,
                                   char const* const category) {
        if (!regions.has_value() || !bytes_per_region.has_value()) {
            return;
        }
        output = checked_multiply(*regions, *bytes_per_region);
        if (!output.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{"Packed access "} + category + " byte count overflows uint64."});
        }
    };
    derive_region_bytes(result.minimum_cache_lines_touched,
                        result.cache_line_bytes,
                        result.minimum_cache_bytes_touched,
                        "cache-line");
    derive_region_bytes(
        result.minimum_pages_touched, result.page_bytes, result.minimum_page_bytes_touched, "page");

    auto classify_regions = [&](bool const selected,
                                std::optional<std::uint64_t> const all_regions,
                                std::optional<std::uint64_t> const bytes_per_region,
                                std::optional<std::uint64_t>& operation_regions,
                                std::optional<std::uint64_t>& operation_bytes,
                                char const* const category) {
        if (!selected) {
            operation_regions = 0;
            operation_bytes = 0;
            return;
        }
        operation_regions = all_regions;
        derive_region_bytes(operation_regions, bytes_per_region, operation_bytes, category);
    };
    classify_regions(read_selected,
                     result.minimum_cache_lines_touched,
                     result.cache_line_bytes,
                     result.read_cache_lines_touched,
                     result.read_cache_bytes_touched,
                     "read cache-line");
    classify_regions(write_selected,
                     result.minimum_cache_lines_touched,
                     result.cache_line_bytes,
                     result.write_cache_lines_touched,
                     result.write_cache_bytes_touched,
                     "write cache-line");
    classify_regions(read_selected,
                     result.minimum_pages_touched,
                     result.page_bytes,
                     result.read_pages_touched,
                     result.read_page_bytes_touched,
                     "read page");
    classify_regions(write_selected,
                     result.minimum_pages_touched,
                     result.page_bytes,
                     result.write_pages_touched,
                     result.write_page_bytes_touched,
                     "write page");

    if (!result.cache_line_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Packed access cache-line footprint is Unknown for ABI profile '" + abi.name() +
                 "'."});
    } else if (!result.minimum_cache_lines_touched.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Packed access cache-line count is Unknown."});
    }
    if (!result.page_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Packed access page footprint is Unknown for ABI profile '" + abi.name() + "'."});
    } else if (!result.minimum_pages_touched.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Packed access page count is Unknown."});
    }
    result.cache_footprint_capacity =
        cache_capacity_analysis(result.minimum_cache_bytes_touched, abi.memory_facts());
    return result;
}

auto Analyzer::compare_packed_access(PackedAccessAnalysis const& first,
                                     PackedAccessAnalysis const& second) -> PackedAccessComparison {
    PackedAccessComparison result;
    result.field_names = first.field_names;
    result.accesses = first.accesses;
    result.element_count = first.element_count;
    result.multiplicity = first.multiplicity;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First packed access: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second packed access: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared packed access analyses describe different packed values."});
        return result;
    }
    if (first.element_count != second.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared packed access analyses use different element counts."});
        return result;
    }
    if (!access_intents_match(first.accesses, second.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared packed access analyses use different per-field access classifications."});
        return result;
    }
    if (first.multiplicity != second.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared packed access analyses use different access multiplicities."});
        return result;
    }
    auto const first_names{StringSet{first.field_names.begin(), first.field_names.end()}};
    auto const second_names{StringSet{second.field_names.begin(), second.field_names.end()}};
    if (first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared packed access analyses do not select the same named fields."});
        return result;
    }
    for (auto const& field_name : result.field_names) {
        auto const first_field{
            std::ranges::find(first.fields, field_name, &PackedFieldAccessAnalysis::name)};
        auto const second_field{
            std::ranges::find(second.fields, field_name, &PackedFieldAccessAnalysis::name)};
        if (first_field == first.fields.end() || second_field == second.fields.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared packed access analysis is missing details for selected field '" +
                     field_name + "'."});
            return result;
        }
        result.fields.push_back(
            {.name = field_name,
             .first = *first_field,
             .second = *second_field,
             .bit_width_delta = numeric_delta(first_field->bit_width, second_field->bit_width),
             .useful_bit_delta = numeric_delta(first_field->useful_bits, second_field->useful_bits),
             .read_useful_bit_delta =
                 numeric_delta(first_field->read_useful_bits, second_field->read_useful_bits),
             .write_useful_bit_delta =
                 numeric_delta(first_field->write_useful_bits, second_field->write_useful_bits),
             .logical_read_useful_bit_delta = numeric_delta(first_field->logical_read_useful_bits,
                                                            second_field->logical_read_useful_bits),
             .logical_write_useful_bit_delta = numeric_delta(
                 first_field->logical_write_useful_bits, second_field->logical_write_useful_bits)});
    }

    result.useful_bit_delta = numeric_delta(first.useful_bits, second.useful_bits);
    result.read_useful_bit_delta = numeric_delta(first.read_useful_bits, second.read_useful_bits);
    result.write_useful_bit_delta =
        numeric_delta(first.write_useful_bits, second.write_useful_bits);
    result.logical_read_useful_bit_delta =
        numeric_delta(first.logical_read_useful_bits, second.logical_read_useful_bits);
    result.logical_write_useful_bit_delta =
        numeric_delta(first.logical_write_useful_bits, second.logical_write_useful_bits);
    result.storage_footprint_byte_delta =
        numeric_delta(first.storage_footprint_bytes, second.storage_footprint_bytes);
    result.storage_footprint_bit_delta =
        numeric_delta(first.storage_footprint_bits, second.storage_footprint_bits);
    result.non_useful_storage_bit_delta =
        numeric_delta(first.non_useful_storage_bits, second.non_useful_storage_bits);
    result.unselected_field_bit_delta =
        numeric_delta(first.unselected_field_bits, second.unselected_field_bits);
    result.reserved_region_bit_delta =
        numeric_delta(first.reserved_region_bits, second.reserved_region_bits);
    result.physically_unused_storage_bit_delta =
        numeric_delta(first.physically_unused_storage_bits, second.physically_unused_storage_bits);
    result.cache_line_size_delta = numeric_delta(first.cache_line_bytes, second.cache_line_bytes);
    result.cache_line_delta =
        numeric_delta(first.minimum_cache_lines_touched, second.minimum_cache_lines_touched);
    result.cache_byte_delta =
        numeric_delta(first.minimum_cache_bytes_touched, second.minimum_cache_bytes_touched);
    result.read_cache_line_delta =
        numeric_delta(first.read_cache_lines_touched, second.read_cache_lines_touched);
    result.read_cache_byte_delta =
        numeric_delta(first.read_cache_bytes_touched, second.read_cache_bytes_touched);
    result.write_cache_line_delta =
        numeric_delta(first.write_cache_lines_touched, second.write_cache_lines_touched);
    result.write_cache_byte_delta =
        numeric_delta(first.write_cache_bytes_touched, second.write_cache_bytes_touched);
    result.page_size_delta = numeric_delta(first.page_bytes, second.page_bytes);
    result.page_delta = numeric_delta(first.minimum_pages_touched, second.minimum_pages_touched);
    result.page_byte_delta =
        numeric_delta(first.minimum_page_bytes_touched, second.minimum_page_bytes_touched);
    result.read_page_delta = numeric_delta(first.read_pages_touched, second.read_pages_touched);
    result.read_page_byte_delta =
        numeric_delta(first.read_page_bytes_touched, second.read_page_bytes_touched);
    result.write_page_delta = numeric_delta(first.write_pages_touched, second.write_pages_touched);
    result.write_page_byte_delta =
        numeric_delta(first.write_page_bytes_touched, second.write_page_bytes_touched);
    return result;
}

} // namespace ioj::layout

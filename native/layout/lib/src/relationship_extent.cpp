#include "relationship_extent.hpp"

#include "analysis_limits.hpp"
#include "checked_arithmetic.hpp"
#include "integer_code_domain.hpp"
#include "relationship_extent_requirement.hpp"
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/relationship_metrics.hpp>

#include <codegen/schema/declarations.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/strings.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>

namespace ioj::layout::analysis_detail {
using lispb::codegen::PackedIntegerValue;
using lispb::codegen::SemanticRelationKind;
using lispb::codegen::to_serialized_string;

auto relationship_extent_requirement(std::uint64_t const extent,
                                     bool const includes_terminal_count,
                                     std::uint64_t const sentinel_codes)
    -> RelationshipExtentRequirement {
    auto const maximum{maximum_analysis_count};
    auto live_is_two_to_64{includes_terminal_count && extent == maximum};
    auto const live_value{live_is_two_to_64 ? std::uint64_t{0}
                                            : extent + (includes_terminal_count ? 1U : 0U)};
    RelationshipExtentRequirement result{};
    result.live_values = ExactCodeCount{.value = live_value, .two_to_64 = live_is_two_to_64};
    if (live_is_two_to_64) {
        if (sentinel_codes == 0) {
            result.required_codes = ExactCodeCount{.value = 0, .two_to_64 = true};
            result.minimum_code_count_bits = 64;
        } else {
            result.minimum_code_count_bits = 65;
        }
        return result;
    }

    auto const remaining{maximum - live_value};
    if (sentinel_codes <= remaining) {
        result.required_codes =
            ExactCodeCount{.value = live_value + sentinel_codes, .two_to_64 = false};
    } else if (sentinel_codes - remaining == 1) {
        result.required_codes = ExactCodeCount{.value = 0, .two_to_64 = true};
    }
    result.minimum_code_count_bits = result.required_codes.has_value()
                                       ? minimum_bits_for_code_count(*result.required_codes)
                                       : 65;
    return result;
}

auto relationship_code_space_extent_limit(std::uint32_t const width,
                                          bool const includes_terminal_count,
                                          std::uint64_t const sentinel_codes)
    -> std::optional<std::uint64_t> {
    if (width == 0 || width > 64) {
        return std::nullopt;
    }
    auto const overhead{
        checked_add(sentinel_codes, includes_terminal_count ? std::uint64_t{1} : 0)};
    if (!overhead.has_value()) {
        return std::nullopt;
    }
    if (width == 64) {
        if (*overhead == 0) {
            return maximum_analysis_count;
        }
        return maximum_analysis_count - (*overhead - 1);
    }

    auto const code_count{std::uint64_t{1} << width};
    if (*overhead > code_count) {
        return std::nullopt;
    }
    return code_count - *overhead;
}

auto relationship_semantic_extent_limit(SemanticRelationKind const kind,
                                        std::optional<PackedIntegerValue> const minimum_value,
                                        std::optional<PackedIntegerValue> const maximum_value)
    -> std::optional<std::uint64_t> {
    if (!minimum_value.has_value() || !maximum_value.has_value() || minimum_value->negative ||
        minimum_value->magnitude != 0 || maximum_value->negative) {
        return std::nullopt;
    }
    if (kind == SemanticRelationKind::count_of) {
        return maximum_value->magnitude;
    }
    if (maximum_value->magnitude == maximum_unsigned_code) {
        return maximum_value->magnitude;
    }
    return maximum_value->magnitude + 1;
}

auto relationship_sentinel_extent_limit(
    lispb::codegen::Declarations const& declarations,
    SemanticRelationKind const kind,
    lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> const named_codes)
    -> std::optional<std::uint64_t> {
    auto limit{maximum_analysis_count};
    for (auto const code_id : declarations.items(named_codes)) {
        auto const code{code_id};
        if (!declarations.packed_named_codes
                 .sentinel[declarations.packed_named_codes.index(code)]) {
            continue;
        }
        if (declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                .negative ||
            (kind == SemanticRelationKind::count_of &&
             declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                     .magnitude == 0)) {
            return std::nullopt;
        }
        auto const candidate{
            kind == SemanticRelationKind::count_of
                ? declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                          .magnitude -
                      1
                : declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                      .magnitude};
        limit = std::min(limit, candidate);
    }
    return limit;
}

auto analyze_unsigned_relationship_extent(
    AnalysisContext& analysis,
    lispb::codegen::Declarations const& declarations,
    std::string_view const context,
    SemanticRelationKind const kind,
    std::uint64_t const target_extent,
    lispb::codegen::SchemaRange<lispb::codegen::PackedNamedCodeId> const named_codes,
    std::uint32_t const width,
    std::optional<PackedIntegerValue> const minimum_value,
    std::optional<PackedIntegerValue> const maximum_value) -> RelationshipMetrics {
    auto const includes_terminal_count{kind == SemanticRelationKind::count_of};
    auto const sentinel_count{static_cast<std::uint64_t>(
        std::ranges::count_if(declarations.items(named_codes), [&](auto id) {
            return declarations.packed_named_codes
                .sentinel[declarations.packed_named_codes.index(id)];
        }))};
    auto const requirement{
        relationship_extent_requirement(target_extent, includes_terminal_count, sentinel_count)};
    auto const capacity_limit{
        relationship_code_space_extent_limit(width, includes_terminal_count, sentinel_count)};
    auto const semantic_limit{
        relationship_semantic_extent_limit(kind, minimum_value, maximum_value)};
    auto const sentinel_limit{relationship_sentinel_extent_limit(declarations, kind, named_codes)};
    auto effective_limit{std::optional<std::uint64_t>{}};
    if (capacity_limit.has_value() && semantic_limit.has_value() && sentinel_limit.has_value()) {
        effective_limit = std::min({*capacity_limit, *semantic_limit, *sentinel_limit});
    }
    RelationshipMetrics result{};
    result.target_extent = target_extent;
    result.live_values = requirement.live_values;
    result.required_codes = requirement.required_codes;
    result.code_space_capacity_limit = capacity_limit;
    result.semantic_capacity_limit = semantic_limit;
    result.sentinel_capacity_limit = sentinel_limit;
    result.effective_capacity_limit = effective_limit;

    auto const has_live_value{includes_terminal_count || target_extent != 0};
    auto const maximum_live_value{
        includes_terminal_count ? target_extent
                                : (target_extent == 0 ? std::uint64_t{0} : target_extent - 1)};
    auto maximum_encoded_value{maximum_live_value};
    for (auto const code_id : declarations.items(named_codes)) {
        auto const code{code_id};
        if (!declarations.packed_named_codes
                 .sentinel[declarations.packed_named_codes.index(code)]) {
            continue;
        }
        maximum_encoded_value = std::max(
            maximum_encoded_value,
            declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                .magnitude);
        auto const collides{
            !declarations.packed_named_codes.value[declarations.packed_named_codes.index(code)]
                 .negative &&
            has_live_value &&
            (includes_terminal_count ? declarations.packed_named_codes
                                               .value[declarations.packed_named_codes.index(code)]
                                               .magnitude <= target_extent
                                     : declarations.packed_named_codes
                                               .value[declarations.packed_named_codes.index(code)]
                                               .magnitude < target_extent)};
        if (collides) {
            analysis.report({DiagnosticSeverity::error,
                             std::string{context} + " sentinel code '" +
                                 std::string{declarations.strings().view(
                                     declarations.packed_named_codes
                                         .name[declarations.packed_named_codes.index(code)])} +
                                 "' collides with the live values required by its '" +
                                 std::string{to_serialized_string(kind)} + "' target extent."});
        }
    }

    auto numeric_bits{std::uint32_t{1}};
    if (has_live_value || sentinel_count != 0) {
        auto const numeric_requirement{lispb::codegen::minimum_packed_integer_bits(
            PackedIntegerValue{0}, PackedIntegerValue{maximum_encoded_value}, false)};
        numeric_bits = numeric_requirement.value_or(65);
    }
    result.minimum_required_bits = std::max(requirement.minimum_code_count_bits, numeric_bits);
    result.width_sufficient =
        result.minimum_required_bits <= 64 && width >= result.minimum_required_bits;
    if (result.width_sufficient && result.code_space_capacity_limit.has_value() &&
        target_extent <= *result.code_space_capacity_limit) {
        result.capacity_headroom = *result.code_space_capacity_limit - target_extent;
    }
    if (result.width_sufficient && result.effective_capacity_limit.has_value() &&
        target_extent <= *result.effective_capacity_limit) {
        result.effective_capacity_headroom = *result.effective_capacity_limit - target_extent;
    }
    if (!result.width_sufficient) {
        analysis.report({DiagnosticSeverity::error,
                         std::string{context} + " uses " + std::to_string(width) +
                             " bits but its '" + std::string{to_serialized_string(kind)} +
                             "' target extent requires at least " +
                             std::to_string(result.minimum_required_bits) + " bits."});
    }
    if (has_live_value && minimum_value.has_value() && maximum_value.has_value() &&
        (minimum_value->negative || minimum_value->magnitude != 0 || maximum_value->negative ||
         maximum_value->magnitude < maximum_live_value)) {
        analysis.report({DiagnosticSeverity::error,
                         std::string{context} +
                             " semantic range does not cover the live values required by its '" +
                             std::string{to_serialized_string(kind)} + "' target extent."});
    }
    return result;
}

}

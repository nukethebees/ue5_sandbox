#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/physical_facts.hpp>

#include <codegen/schema.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <string_view>
#include <utility>

namespace ioj::layout {
namespace {

inline auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

inline auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}

inline auto maximum_unsigned_value(std::uint32_t const bits) -> std::optional<std::uint64_t> {
    if (bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (std::uint64_t{1} << bits) - 1;
}

inline auto minimum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(std::uint64_t{1} << (bits - 1));
}

inline auto maximum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>((std::uint64_t{1} << (bits - 1)) - 1);
}

inline auto minimum_bits_for_code_count(ExactCodeCount const count) -> std::uint32_t {
    if (count.two_to_64) {
        return 64;
    }
    auto remaining{count.value > 1 ? count.value - 1 : 0};
    std::uint32_t bits{1};
    while (remaining > 1) {
        remaining >>= 1;
        ++bits;
    }
    return bits;
}

struct RelationshipExtentRequirement {
    std::optional<ExactCodeCount> live_values;
    std::optional<ExactCodeCount> required_codes;
    std::uint32_t minimum_code_count_bits{1};
};

inline auto relationship_extent_requirement(std::uint64_t const extent,
                                            bool const includes_terminal_count,
                                            std::uint64_t const sentinel_codes)
    -> RelationshipExtentRequirement {
    auto const maximum{(std::numeric_limits<std::uint64_t>::max)()};
    auto live_is_two_to_64{includes_terminal_count && extent == maximum};
    auto const live_value{live_is_two_to_64 ? std::uint64_t{0}
                                            : extent + (includes_terminal_count ? 1U : 0U)};
    RelationshipExtentRequirement result{
        .live_values = ExactCodeCount{.value = live_value, .two_to_64 = live_is_two_to_64},
        .required_codes = std::nullopt,
        .minimum_code_count_bits = 1};
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

inline auto relationship_code_space_extent_limit(std::uint32_t const width,
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
            return (std::numeric_limits<std::uint64_t>::max)();
        }
        return (std::numeric_limits<std::uint64_t>::max)() - (*overhead - 1);
    }

    auto const code_count{std::uint64_t{1} << width};
    if (*overhead > code_count) {
        return std::nullopt;
    }
    return code_count - *overhead;
}

inline auto relationship_semantic_extent_limit(
    codegen::SemanticRelationKind const kind,
    std::optional<codegen::PackedIntegerValue> const minimum_value,
    std::optional<codegen::PackedIntegerValue> const maximum_value)
    -> std::optional<std::uint64_t> {
    if (!minimum_value.has_value() || !maximum_value.has_value() || minimum_value->negative ||
        minimum_value->magnitude != 0 || maximum_value->negative) {
        return std::nullopt;
    }
    if (kind == codegen::SemanticRelationKind::count_of) {
        return maximum_value->magnitude;
    }
    if (maximum_value->magnitude == (std::numeric_limits<std::uint64_t>::max)()) {
        return maximum_value->magnitude;
    }
    return maximum_value->magnitude + 1;
}

inline auto relationship_sentinel_extent_limit(
    codegen::SemanticRelationKind const kind,
    std::span<lispb::schema::PackedNamedCode const> const named_codes)
    -> std::optional<std::uint64_t> {
    auto limit{(std::numeric_limits<std::uint64_t>::max)()};
    for (auto const& code : named_codes) {
        if (!code.sentinel) {
            continue;
        }
        if (code.value.negative ||
            (kind == codegen::SemanticRelationKind::count_of && code.value.magnitude == 0)) {
            return std::nullopt;
        }
        auto const candidate{kind == codegen::SemanticRelationKind::count_of
                                 ? code.value.magnitude - 1
                                 : code.value.magnitude};
        limit = std::min(limit, candidate);
    }
    return limit;
}

struct RelationshipExtentAnalysis {
    std::uint64_t target_extent{};
    std::optional<ExactCodeCount> live_values;
    std::optional<ExactCodeCount> required_codes;
    std::uint32_t minimum_required_bits{1};
    bool width_sufficient{};
    std::optional<std::uint64_t> code_space_capacity_limit;
    std::optional<std::uint64_t> capacity_headroom;
    std::optional<std::uint64_t> semantic_capacity_limit;
    std::optional<std::uint64_t> sentinel_capacity_limit;
    std::optional<std::uint64_t> effective_capacity_limit;
    std::optional<std::uint64_t> effective_capacity_headroom;
    std::vector<Diagnostic> diagnostics;
};

inline auto analyze_unsigned_relationship_extent(
    std::string_view const context,
    codegen::SemanticRelationKind const kind,
    std::uint64_t const target_extent,
    std::span<lispb::schema::PackedNamedCode const> const named_codes,
    std::uint32_t const width,
    std::optional<codegen::PackedIntegerValue> const minimum_value,
    std::optional<codegen::PackedIntegerValue> const maximum_value) -> RelationshipExtentAnalysis {
    auto const includes_terminal_count{kind == codegen::SemanticRelationKind::count_of};
    auto const sentinel_count{static_cast<std::uint64_t>(
        std::ranges::count(named_codes, true, &lispb::schema::PackedNamedCode::sentinel))};
    auto const requirement{
        relationship_extent_requirement(target_extent, includes_terminal_count, sentinel_count)};
    auto const capacity_limit{
        relationship_code_space_extent_limit(width, includes_terminal_count, sentinel_count)};
    auto const semantic_limit{
        relationship_semantic_extent_limit(kind, minimum_value, maximum_value)};
    auto const sentinel_limit{relationship_sentinel_extent_limit(kind, named_codes)};
    auto effective_limit{std::optional<std::uint64_t>{}};
    if (capacity_limit.has_value() && semantic_limit.has_value() && sentinel_limit.has_value()) {
        effective_limit = std::min({*capacity_limit, *semantic_limit, *sentinel_limit});
    }
    auto result{RelationshipExtentAnalysis{.target_extent = target_extent,
                                           .live_values = requirement.live_values,
                                           .required_codes = requirement.required_codes,
                                           .minimum_required_bits = 1,
                                           .width_sufficient = false,
                                           .code_space_capacity_limit = capacity_limit,
                                           .capacity_headroom = std::nullopt,
                                           .semantic_capacity_limit = semantic_limit,
                                           .sentinel_capacity_limit = sentinel_limit,
                                           .effective_capacity_limit = effective_limit,
                                           .effective_capacity_headroom = std::nullopt,
                                           .diagnostics = {}}};

    auto const has_live_value{includes_terminal_count || target_extent != 0};
    auto const maximum_live_value{
        includes_terminal_count ? target_extent
                                : (target_extent == 0 ? std::uint64_t{0} : target_extent - 1)};
    auto maximum_encoded_value{maximum_live_value};
    for (auto const& code : named_codes) {
        if (!code.sentinel) {
            continue;
        }
        maximum_encoded_value = std::max(maximum_encoded_value, code.value.magnitude);
        auto const collides{!code.value.negative && has_live_value &&
                            (includes_terminal_count ? code.value.magnitude <= target_extent
                                                     : code.value.magnitude < target_extent)};
        if (collides) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{context} + " sentinel code '" + code.name +
                     "' collides with the live values required by its '" +
                     std::string{codegen::semantic_relation_kind_name(kind)} + "' target extent."});
        }
    }

    auto numeric_bits{std::uint32_t{1}};
    if (has_live_value || sentinel_count != 0) {
        auto const numeric_requirement{
            codegen::minimum_packed_integer_bits(codegen::PackedIntegerValue{0},
                                                 codegen::PackedIntegerValue{maximum_encoded_value},
                                                 false)};
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
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      std::string{context} + " uses " + std::to_string(width) +
                                          " bits but its '" +
                                          std::string{codegen::semantic_relation_kind_name(kind)} +
                                          "' target extent requires at least " +
                                          std::to_string(result.minimum_required_bits) + " bits."});
    }
    if (has_live_value && minimum_value.has_value() && maximum_value.has_value() &&
        (minimum_value->negative || minimum_value->magnitude != 0 || maximum_value->negative ||
         maximum_value->magnitude < maximum_live_value)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             std::string{context} +
                 " semantic range does not cover the live values required by its '" +
                 std::string{codegen::semantic_relation_kind_name(kind)} + "' target extent."});
    }
    return result;
}

inline auto relationship_target_extent(lispb::schema::SemanticRelationship const& relationship,
                                       RelationshipTargetFacts const& target_facts)
    -> std::optional<std::uint64_t> {
    if (relationship.kind == codegen::SemanticRelationKind::index_into ||
        relationship.kind == codegen::SemanticRelationKind::count_of) {
        return target_facts.element_capacity;
    }
    if (relationship.kind != codegen::SemanticRelationKind::offset_into ||
        !relationship.unit.has_value()) {
        return std::nullopt;
    }
    return *relationship.unit == codegen::SemanticRelationUnit::elements
             ? target_facts.element_capacity
             : target_facts.byte_extent;
}

inline auto packed_range_count(codegen::PackedIntegerValue const minimum,
                               codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t> {
    if (codegen::packed_integer_less(maximum, minimum)) {
        return std::nullopt;
    }
    if (minimum.negative && maximum.negative) {
        return checked_add(minimum.magnitude - maximum.magnitude, 1);
    }
    if (!minimum.negative && !maximum.negative) {
        return checked_add(maximum.magnitude - minimum.magnitude, 1);
    }
    auto const through_zero{checked_add(minimum.magnitude, maximum.magnitude)};
    return through_zero.has_value() ? checked_add(*through_zero, 1) : std::nullopt;
}

inline auto packed_range_span(codegen::PackedIntegerValue const minimum,
                              codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t> {
    if (codegen::packed_integer_less(maximum, minimum)) {
        return std::nullopt;
    }
    if (minimum.negative && maximum.negative) {
        return minimum.magnitude - maximum.magnitude;
    }
    if (!minimum.negative && !maximum.negative) {
        return maximum.magnitude - minimum.magnitude;
    }
    return checked_add(minimum.magnitude, maximum.magnitude);
}

inline auto packed_integer_as_long_double(codegen::PackedIntegerValue const value) -> long double {
    auto const magnitude{static_cast<long double>(value.magnitude)};
    return value.negative ? -magnitude : magnitude;
}

inline auto cache_capacity_analysis(std::optional<std::uint64_t> const working_set_bytes,
                                    MemoryFacts const& memory) -> CacheCapacityAnalysis {
    auto fits =
        [working_set_bytes](std::optional<std::uint64_t> const capacity) -> std::optional<bool> {
        if (!working_set_bytes.has_value() || !capacity.has_value()) {
            return std::nullopt;
        }
        return *working_set_bytes <= *capacity;
    };
    return {.working_set_bytes = working_set_bytes,
            .l1_data_capacity_bytes = memory.l1_data_cache_bytes,
            .l2_capacity_bytes = memory.l2_cache_bytes,
            .l3_capacity_bytes = memory.l3_cache_bytes,
            .fits_l1_data = fits(memory.l1_data_cache_bytes),
            .fits_l2 = fits(memory.l2_cache_bytes),
            .fits_l3 = fits(memory.l3_cache_bytes)};
}

inline auto enum_domain_fits_backing(EnumCodeValue const minimum,
                                     EnumCodeValue const maximum,
                                     TypeFacts const& facts,
                                     std::uint64_t const backing_bits) -> std::optional<bool> {
    if (!facts.integer_signed.has_value()) {
        return std::nullopt;
    }
    if (!*facts.integer_signed) {
        if (!facts.unsigned_value_bits.has_value()) {
            return std::nullopt;
        }
        auto const maximum_value{maximum_unsigned_value(*facts.unsigned_value_bits)};
        return maximum_value.has_value() && !minimum.negative && !maximum.negative &&
               maximum.magnitude <= *maximum_value;
    }
    if (backing_bits == 0 || backing_bits > 64) {
        return std::nullopt;
    }
    auto const negative_magnitude_limit{
        backing_bits == 64 ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (backing_bits - 1))};
    auto const positive_limit{negative_magnitude_limit - 1};
    auto const minimum_fits{!minimum.negative || minimum.magnitude <= negative_magnitude_limit};
    auto const maximum_fits{maximum.negative || maximum.magnitude <= positive_limit};
    return minimum_fits && maximum_fits;
}

inline auto effective_field_width(lispb::schema::TypeId const type,
                                  lispb::schema::PackedField const& field,
                                  Variant const& variant) -> std::uint32_t {
    auto const found{variant.overrides.packed_field_widths.find(
        FieldOverrideId{.type = type, .field_name = field.name})};
    return found == variant.overrides.packed_field_widths.end() ? field.bit_width : found->second;
}

inline auto effective_storage_type(lispb::schema::TypeGraph const& types,
                                   lispb::schema::TypeId const type,
                                   lispb::schema::PackedType const& packed,
                                   Variant const& variant) -> std::string {
    auto const found{variant.overrides.packed_storage_types.find(type)};
    if (found != variant.overrides.packed_storage_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, packed.storage_type)
        .value_or(packed.storage_type.cpp_type.spelling);
}

inline auto effective_column_type(lispb::schema::TypeGraph const& types,
                                  lispb::schema::TypeId const type,
                                  lispb::schema::SoaColumn const& column,
                                  Variant const& variant) -> std::string {
    auto const found{variant.overrides.soa_column_types.find(
        FieldOverrideId{.type = type, .field_name = column.name})};
    if (found != variant.overrides.soa_column_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, column.semantic_type)
        .value_or(column.semantic_type.cpp_type.spelling);
}

inline auto minimum_regions(std::uint64_t const bytes, std::uint64_t const region_bytes)
    -> std::uint64_t {
    return bytes / region_bytes + (bytes % region_bytes == 0 ? 0 : 1);
}

inline auto classified_useful_bytes(std::optional<std::uint64_t> const useful_bytes,
                                    bool const included) -> std::optional<std::uint64_t> {
    return included ? useful_bytes : std::optional<std::uint64_t>{std::uint64_t{}};
}

inline auto logical_useful_bytes(std::optional<std::uint64_t> const classified_bytes,
                                 std::uint64_t const multiplicity) -> std::optional<std::uint64_t> {
    if (multiplicity == 0 || !classified_bytes.has_value()) {
        return std::nullopt;
    }
    return checked_multiply(*classified_bytes, multiplicity);
}

inline auto access_includes_read(AccessOperation const operation) -> bool {
    return operation != AccessOperation::write;
}

inline auto access_includes_write(AccessOperation const operation) -> bool {
    return operation != AccessOperation::read;
}

inline auto accept_unique_access(std::set<std::string, std::less<>>& names,
                                 std::span<AccessIntent const> const accepted,
                                 std::vector<Diagnostic>& diagnostics,
                                 AccessIntent const& access,
                                 std::string_view const member_kind) -> bool {
    if (names.insert(access.name).second) {
        return true;
    }
    auto const existing{std::ranges::find(accepted, access.name, &AccessIntent::name)};
    if (existing != accepted.end() && existing->operation != access.operation) {
        diagnostics.push_back({DiagnosticSeverity::error,
                               "Selected " + std::string{member_kind} + " '" + access.name +
                                   "' has conflicting access classifications; the first is used."});
    }
    return false;
}

inline auto access_intents_match(std::span<AccessIntent const> const first,
                                 std::span<AccessIntent const> const second) -> bool {
    if (first.size() != second.size()) {
        return false;
    }
    return std::ranges::all_of(first, [&](AccessIntent const& access) {
        auto const match{std::ranges::find(second, access.name, &AccessIntent::name)};
        return match != second.end() && match->operation == access.operation;
    });
}

inline auto straddling_elements(std::uint64_t const element_bytes,
                                std::uint64_t const element_count,
                                std::uint64_t const region_bytes,
                                std::optional<std::uint64_t> const total_bytes)
    -> std::optional<std::uint64_t> {
    if (element_count == 0) {
        return 0;
    }
    if (element_bytes == 0) {
        return std::nullopt;
    }
    if (element_bytes > region_bytes) {
        return element_count;
    }
    if (!total_bytes.has_value()) {
        return std::nullopt;
    }

    auto const boundaries_before_end{(*total_bytes - 1) / region_bytes};
    auto const coincident_boundary_period{region_bytes / std::gcd(element_bytes, region_bytes)};
    auto const coincident_boundaries{(element_count - 1) / coincident_boundary_period};
    return boundaries_before_end - coincident_boundaries;
}

inline auto straddling_elements_at_offset(std::uint64_t const element_bytes,
                                          std::uint64_t const element_count,
                                          std::uint64_t const region_bytes,
                                          std::optional<std::uint64_t> const total_bytes,
                                          std::uint64_t const allocation_offset)
    -> std::optional<std::uint64_t> {
    if (element_count == 0) {
        return 0;
    }
    if (element_bytes == 0 || !total_bytes.has_value()) {
        return std::nullopt;
    }
    if (element_bytes > region_bytes) {
        return element_count;
    }

    auto const period{region_bytes / std::gcd(element_bytes, region_bytes)};
    constexpr std::uint64_t maximum_period{1'000'000};
    auto const elements_to_analyze{std::min(element_count, period)};
    if (elements_to_analyze > maximum_period) {
        return std::nullopt;
    }

    auto offset_in_region{allocation_offset % region_bytes};
    std::uint64_t straddles_in_analyzed_elements{};
    std::uint64_t straddles_in_remainder{};
    auto const remainder{element_count % period};
    for (std::uint64_t index{}; index < elements_to_analyze; ++index) {
        auto const straddles{offset_in_region > region_bytes - element_bytes};
        straddles_in_analyzed_elements += straddles ? 1U : 0U;
        if (index < remainder) {
            straddles_in_remainder += straddles ? 1U : 0U;
        }
        if (offset_in_region >= region_bytes - element_bytes) {
            offset_in_region -= region_bytes - element_bytes;
        } else {
            offset_in_region += element_bytes;
        }
    }
    if (element_count <= period) {
        return straddles_in_analyzed_elements;
    }

    auto const full_periods{element_count / period};
    auto const full_period_straddles{
        checked_multiply(straddles_in_analyzed_elements, full_periods)};
    return full_period_straddles.has_value()
             ? checked_add(*full_period_straddles, straddles_in_remainder)
             : std::nullopt;
}

inline auto touched_regions(std::uint64_t const stride_bytes,
                            std::span<std::pair<std::uint64_t, std::uint64_t> const> const members,
                            std::uint64_t const element_count,
                            std::uint64_t const region_bytes,
                            std::string& error) -> std::optional<std::uint64_t> {
    if (element_count == 0 || members.empty()) {
        return 0;
    }
    auto intervals = [&](std::uint64_t const index)
        -> std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> {
        auto const base{checked_multiply(index, stride_bytes)};
        if (!base.has_value()) {
            return std::nullopt;
        }
        std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
        result.reserve(members.size());
        for (auto const& [offset, extent] : members) {
            auto const begin{checked_add(*base, offset)};
            auto const end{begin.has_value() ? checked_add(*begin, extent) : std::nullopt};
            if (!end.has_value() || *end == 0) {
                return std::nullopt;
            }
            result.emplace_back(*begin / region_bytes, (*end - 1) / region_bytes);
        }
        return result;
    };
    auto contribution =
        [&](std::uint64_t const index,
            std::optional<std::uint64_t>& previous_end) -> std::optional<std::uint64_t> {
        auto const current{intervals(index)};
        if (!current.has_value()) {
            return std::nullopt;
        }
        std::uint64_t added{};
        for (auto const& [begin, end] : *current) {
            auto interval_added{std::uint64_t{}};
            if (!previous_end.has_value() || begin > *previous_end) {
                interval_added = end - begin + 1;
            } else if (end > *previous_end) {
                interval_added = end - *previous_end;
            }
            auto const next{checked_add(added, interval_added)};
            if (!next.has_value()) {
                return std::nullopt;
            }
            added = *next;
            previous_end = std::max(previous_end.value_or(0), end);
        }
        return added;
    };
    std::optional<std::uint64_t> previous_end;
    auto total{contribution(0, previous_end)};
    if (!total.has_value()) {
        error = "Member access address arithmetic overflows uint64.";
        return std::nullopt;
    }
    if (element_count == 1) {
        return total;
    }

    auto const period{region_bytes / std::gcd(stride_bytes, region_bytes)};
    constexpr std::uint64_t maximum_period{1'000'000};
    if (period > maximum_period) {
        error = "Target region/stride period is too large for exact access analysis.";
        return std::nullopt;
    }
    auto const transition_count{element_count - 1};
    auto const complete_periods{transition_count / period};
    auto const remaining_transitions{transition_count % period};
    auto const transitions_to_analyze{complete_periods == 0 ? remaining_transitions : period};
    std::uint64_t period_contribution{};
    std::uint64_t remainder_contribution{};
    for (std::uint64_t index{1}; index <= transitions_to_analyze; ++index) {
        auto const added{contribution(index, previous_end)};
        if (!added.has_value()) {
            error = "Member access address arithmetic overflows uint64.";
            return std::nullopt;
        }
        auto const next_period{checked_add(period_contribution, *added)};
        if (!next_period.has_value()) {
            error = "Member access region count overflows uint64.";
            return std::nullopt;
        }
        period_contribution = *next_period;
        if (index <= remaining_transitions) {
            auto const next_remainder{checked_add(remainder_contribution, *added)};
            if (!next_remainder.has_value()) {
                error = "Member access region count overflows uint64.";
                return std::nullopt;
            }
            remainder_contribution = *next_remainder;
        }
    }

    auto const periods_contribution{checked_multiply(complete_periods, period_contribution)};
    if (!periods_contribution.has_value()) {
        error = "Member access region count overflows uint64.";
        return std::nullopt;
    }
    total = checked_add(*total, *periods_contribution);
    total = total.has_value() ? checked_add(*total, remainder_contribution) : std::nullopt;
    if (!total.has_value()) {
        error = "Member access region count overflows uint64.";
    }
    return total;
}

inline auto cache_line_tiling(std::uint64_t const element_bytes,
                              std::uint64_t const cache_line_bytes) -> CacheLineTiling {
    auto const complete_elements{
        element_bytes <= cache_line_bytes ? cache_line_bytes / element_bytes : std::uint64_t{0}};
    return {.cache_line_bytes = cache_line_bytes,
            .element_bytes = element_bytes,
            .exact_elements_per_cache_line =
                element_bytes <= cache_line_bytes && cache_line_bytes % element_bytes == 0
                    ? std::optional<std::uint64_t>{complete_elements}
                    : std::nullopt,
            .complete_elements_from_line_start = complete_elements,
            .boundary_fragment_bytes =
                element_bytes <= cache_line_bytes ? cache_line_bytes % element_bytes : 0,
            .minimum_cache_lines_per_element = minimum_regions(element_bytes, cache_line_bytes)};
}

inline auto resolved_enum_domain(lispb::schema::EnumType const& enumeration)
    -> lispb::schema::EnumDomain {
    std::vector<lispb::schema::EnumDomainInput> values;
    values.reserve(enumeration.enumerators.size());
    for (auto const& enumerator : enumeration.enumerators) {
        values.push_back({.name = enumerator.name,
                          .explicit_value = enumerator.explicit_value,
                          .reserved = enumerator.sentinel || enumerator.count_sentinel});
    }
    return lispb::schema::analyze_enum_domain(values, enumeration.signedness);
}

inline auto derived_enum_backing_type(lispb::schema::EnumType const& enumeration)
    -> std::optional<std::string> {
    auto const domain{resolved_enum_domain(enumeration)};
    auto const requirement{
        lispb::schema::derive_enum_storage_requirement(domain, enumeration.bit_width)};
    if (!requirement.has_value()) {
        return std::nullopt;
    }
    return "std::" + std::string{requirement->signedness ? "int" : "uint"} +
           std::to_string(requirement->bit_width) + "_t";
}

} // namespace
namespace {

inline auto align_up(std::uint64_t const value, std::uint64_t const alignment)
    -> std::optional<std::uint64_t> {
    if (alignment == 0) {
        return std::nullopt;
    }
    auto const remainder{value % alignment};
    return remainder == 0 ? std::optional{value} : checked_add(value, alignment - remainder);
}

inline auto exact_code_count_delta(ExactCodeCount const baseline, ExactCodeCount const variant)
    -> NumericDelta {
    if (baseline == variant) {
        return {.direction = NumericDeltaDirection::unchanged, .magnitude = 0, .percentage = 0.0};
    }

    auto const increased{variant.two_to_64 ||
                         (!baseline.two_to_64 && variant.value > baseline.value)};
    auto const& smaller{increased ? baseline : variant};
    auto const& larger{increased ? variant : baseline};
    auto const magnitude{larger.two_to_64
                             ? (std::numeric_limits<std::uint64_t>::max)() - smaller.value + 1
                             : larger.value - smaller.value};
    auto const baseline_value{baseline.two_to_64 ? std::ldexp(1.0L, 64)
                                                 : static_cast<long double>(baseline.value)};
    auto const percentage{
        static_cast<double>(static_cast<long double>(magnitude) * 100.0L / baseline_value)};
    return {.direction =
                increased ? NumericDeltaDirection::increased : NumericDeltaDirection::decreased,
            .magnitude = magnitude,
            .percentage = percentage};
}

inline auto unsigned_varint_bytes(std::uint64_t value) -> std::uint32_t {
    auto bytes{std::uint32_t{1}};
    while (value >= 0x80) {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

inline auto signed_integer_value(codegen::PackedIntegerValue const value) -> std::int64_t {
    if (!value.negative) {
        return static_cast<std::int64_t>(value.magnitude);
    }
    if (value.magnitude == (std::uint64_t{1} << 63)) {
        return (std::numeric_limits<std::int64_t>::min)();
    }
    return -static_cast<std::int64_t>(value.magnitude);
}

inline auto signed_varint_bytes(std::int64_t value) -> std::uint32_t {
    auto bytes{std::uint32_t{0}};
    while (true) {
        auto const encoded{static_cast<std::uint8_t>(value & 0x7f)};
        auto const sign_bit_set{(encoded & 0x40) != 0};
        value >>= 7;
        ++bytes;
        if ((value == 0 && !sign_bit_set) || (value == -1 && sign_bit_set)) {
            return bytes;
        }
    }
}

inline auto zigzag_value(codegen::PackedIntegerValue const value) -> std::uint64_t {
    return value.negative ? value.magnitude * 2 - 1 : value.magnitude * 2;
}

inline auto integer_varint_bytes(codegen::PackedIntegerValue const value,
                                 codegen::IntegerVarintEncoding const encoding) -> std::uint32_t {
    switch (encoding) {
        case codegen::IntegerVarintEncoding::unsigned_varint:
            return unsigned_varint_bytes(value.magnitude);
        case codegen::IntegerVarintEncoding::signed_varint:
            return signed_varint_bytes(signed_integer_value(value));
        case codegen::IntegerVarintEncoding::zigzag_varint:
            return unsigned_varint_bytes(zigzag_value(value));
    }
    return 0;
}

} // namespace

} // namespace ioj::layout

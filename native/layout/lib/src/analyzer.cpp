#include <ioj/layout/analyzer.hpp>

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

auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}

auto maximum_unsigned_value(std::uint32_t const bits) -> std::optional<std::uint64_t> {
    if (bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (std::uint64_t{1} << bits) - 1;
}

auto minimum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(std::uint64_t{1} << (bits - 1));
}

auto maximum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>((std::uint64_t{1} << (bits - 1)) - 1);
}

auto minimum_bits_for_code_count(ExactCodeCount const count) -> std::uint32_t {
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

auto relationship_extent_requirement(std::uint64_t const extent,
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

auto relationship_semantic_extent_limit(
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

auto relationship_sentinel_extent_limit(
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

auto analyze_unsigned_relationship_extent(
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

auto relationship_target_extent(lispb::schema::SemanticRelationship const& relationship,
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

auto packed_range_count(codegen::PackedIntegerValue const minimum,
                        codegen::PackedIntegerValue const maximum) -> std::optional<std::uint64_t> {
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

auto packed_range_span(codegen::PackedIntegerValue const minimum,
                       codegen::PackedIntegerValue const maximum) -> std::optional<std::uint64_t> {
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

auto packed_integer_as_long_double(codegen::PackedIntegerValue const value) -> long double {
    auto const magnitude{static_cast<long double>(value.magnitude)};
    return value.negative ? -magnitude : magnitude;
}

auto cache_capacity_analysis(std::optional<std::uint64_t> const working_set_bytes,
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

auto enum_domain_fits_backing(EnumCodeValue const minimum,
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

auto effective_field_width(lispb::schema::TypeId const type,
                           lispb::schema::PackedField const& field,
                           Variant const& variant) -> std::uint32_t {
    auto const found{variant.overrides.packed_field_widths.find(
        FieldOverrideId{.type = type, .field_name = field.name})};
    return found == variant.overrides.packed_field_widths.end() ? field.bit_width : found->second;
}

auto effective_storage_type(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            lispb::schema::PackedType const& packed,
                            Variant const& variant) -> std::string {
    auto const found{variant.overrides.packed_storage_types.find(type)};
    if (found != variant.overrides.packed_storage_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, packed.storage_type.type)
        .value_or(packed.storage_type.cpp_type.spelling);
}

auto effective_column_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           lispb::schema::SoaColumn const& column,
                           Variant const& variant) -> std::string {
    auto const found{variant.overrides.soa_column_types.find(
        FieldOverrideId{.type = type, .field_name = column.name})};
    if (found != variant.overrides.soa_column_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, column.semantic_type.type)
        .value_or(column.semantic_type.cpp_type.spelling);
}

auto minimum_regions(std::uint64_t const bytes, std::uint64_t const region_bytes) -> std::uint64_t {
    return bytes / region_bytes + (bytes % region_bytes == 0 ? 0 : 1);
}

auto classified_useful_bytes(std::optional<std::uint64_t> const useful_bytes, bool const included)
    -> std::optional<std::uint64_t> {
    return included ? useful_bytes : std::optional<std::uint64_t>{std::uint64_t{}};
}

auto logical_useful_bytes(std::optional<std::uint64_t> const classified_bytes,
                          std::uint64_t const multiplicity) -> std::optional<std::uint64_t> {
    if (multiplicity == 0 || !classified_bytes.has_value()) {
        return std::nullopt;
    }
    return checked_multiply(*classified_bytes, multiplicity);
}

auto access_intents_match(std::span<AccessIntent const> const first,
                          std::span<AccessIntent const> const second) -> bool {
    if (first.size() != second.size()) {
        return false;
    }
    return std::ranges::all_of(first, [&](AccessIntent const& access) {
        auto const match{std::ranges::find(second, access.name, &AccessIntent::name)};
        return match != second.end() && match->operation == access.operation;
    });
}

auto straddling_elements(std::uint64_t const element_bytes,
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

auto straddling_elements_at_offset(std::uint64_t const element_bytes,
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

auto touched_regions(std::uint64_t const stride_bytes,
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

auto cache_line_tiling(std::uint64_t const element_bytes, std::uint64_t const cache_line_bytes)
    -> CacheLineTiling {
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

auto resolved_enum_domain(lispb::schema::EnumType const& enumeration) -> lispb::schema::EnumDomain {
    std::vector<lispb::schema::EnumDomainInput> values;
    values.reserve(enumeration.enumerators.size());
    for (auto const& enumerator : enumeration.enumerators) {
        values.push_back({.name = enumerator.name,
                          .explicit_value = enumerator.explicit_value,
                          .reserved = enumerator.sentinel || enumerator.count_sentinel});
    }
    return lispb::schema::analyze_enum_domain(values, enumeration.signedness);
}

auto derived_enum_backing_type(lispb::schema::EnumType const& enumeration)
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

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<std::string> {
    std::vector<lispb::schema::TypeId> visited;
    auto current{type};
    while (current.valid() && std::ranges::find(visited, current) == visited.end()) {
        visited.push_back(current);
        auto const& node{types.type(current)};
        if (auto const* external{std::get_if<lispb::schema::ExternalType>(&node.definition)}) {
            return codegen::native_spelling(external->cpp_type.spelling);
        }
        if (auto const* enumeration{std::get_if<lispb::schema::EnumType>(&node.definition)}) {
            if (enumeration->underlying_type.has_value()) {
                current = enumeration->underlying_type->type;
                continue;
            }
            return derived_enum_backing_type(*enumeration);
        }
        if (auto const* packed{std::get_if<lispb::schema::PackedType>(&node.definition)}) {
            current = packed->storage_type.type;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

namespace {

auto align_up(std::uint64_t const value, std::uint64_t const alignment)
    -> std::optional<std::uint64_t> {
    if (alignment == 0) {
        return std::nullopt;
    }
    auto const remainder{value % alignment};
    return remainder == 0 ? std::optional{value} : checked_add(value, alignment - remainder);
}

class AggregateLayoutAnalyzer {
  public:
    AggregateLayoutAnalyzer(lispb::schema::TypeGraph const& types, AbiProfile const& abi)
        : types_{types}
        , abi_{abi} {}

    auto analyze_record(lispb::schema::TypeId const type) -> RecordAnalysis {
        std::vector<lispb::schema::TypeId> active;
        return analyze_record(type, active);
    }

    auto analyze_union(lispb::schema::TypeId const type) -> UnionAnalysis {
        std::vector<lispb::schema::TypeId> active;
        return analyze_union(type, active);
    }

    auto analyze_tagged_union(lispb::schema::TypeId const type) -> TaggedUnionAnalysis {
        std::vector<lispb::schema::TypeId> active;
        return analyze_tagged_union(type, active);
    }
  private:
    auto facts_for(lispb::schema::TypeId const type,
                   std::vector<lispb::schema::TypeId>& active,
                   std::vector<Diagnostic>& diagnostics,
                   std::string const& context) -> std::optional<TypeFacts> {
        auto const& node{types_.type(type)};
        if (std::holds_alternative<lispb::schema::RecordType>(node.definition)) {
            auto nested{analyze_record(type, active)};
            for (auto& diagnostic : nested.diagnostics) {
                diagnostics.push_back(
                    {diagnostic.severity, context + ": " + std::move(diagnostic.message)});
            }
            if (!nested.size_bytes.has_value() || !nested.alignment_bytes.has_value()) {
                return std::nullopt;
            }
            return TypeFacts{.size_bytes = *nested.size_bytes,
                             .alignment_bytes = *nested.alignment_bytes,
                             .integer_signed = std::nullopt,
                             .unsigned_value_bits = std::nullopt,
                             .provenance = "Derived from target member facts"};
        }
        if (std::holds_alternative<lispb::schema::UnionType>(node.definition)) {
            auto nested{analyze_union(type, active)};
            for (auto& diagnostic : nested.diagnostics) {
                diagnostics.push_back(
                    {diagnostic.severity, context + ": " + std::move(diagnostic.message)});
            }
            if (!nested.size_bytes.has_value() || !nested.alignment_bytes.has_value()) {
                return std::nullopt;
            }
            return TypeFacts{.size_bytes = *nested.size_bytes,
                             .alignment_bytes = *nested.alignment_bytes,
                             .integer_signed = std::nullopt,
                             .unsigned_value_bits = std::nullopt,
                             .provenance = "Derived from target alternative facts"};
        }
        if (std::holds_alternative<lispb::schema::TaggedUnionType>(node.definition)) {
            auto nested{analyze_tagged_union(type, active)};
            for (auto& diagnostic : nested.diagnostics) {
                diagnostics.push_back(
                    {diagnostic.severity, context + ": " + std::move(diagnostic.message)});
            }
            if (!nested.size_bytes.has_value() || !nested.alignment_bytes.has_value()) {
                return std::nullopt;
            }
            return TypeFacts{.size_bytes = *nested.size_bytes,
                             .alignment_bytes = *nested.alignment_bytes,
                             .integer_signed = std::nullopt,
                             .unsigned_value_bits = std::nullopt,
                             .provenance = "Derived from tagged aggregate target facts"};
        }
        auto const spelling{physical_type_spelling(types_, type)};
        if (!spelling.has_value()) {
            diagnostics.push_back(
                {DiagnosticSeverity::error,
                 context + " has no target-layout representation in this analyzer."});
            return std::nullopt;
        }
        auto const facts{abi_.find(*spelling)};
        if (!facts.has_value()) {
            diagnostics.push_back(
                {DiagnosticSeverity::error,
                 context + " has unknown physical facts for type '" + *spelling + "'."});
        }
        return facts;
    }

    auto analyze_record(lispb::schema::TypeId const type,
                        std::vector<lispb::schema::TypeId>& active) -> RecordAnalysis {
        RecordAnalysis result{.type = type,
                              .members = {},
                              .payload_bytes = std::nullopt,
                              .internal_padding_bytes = std::nullopt,
                              .tail_padding_bytes = std::nullopt,
                              .size_bytes = std::nullopt,
                              .alignment_bytes = std::nullopt,
                              .diagnostics = {},
                              .aggregate = {}};
        auto const& node{types_.type(type)};
        auto const* record{std::get_if<lispb::schema::RecordType>(&node.definition)};
        if (record == nullptr) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected semantic type is not a record."});
            return result;
        }
        if (std::ranges::find(active, type) != active.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Illegal by-value record cycle involving '" + node.identity.name + "'."});
            return result;
        }
        active.push_back(type);

        std::uint64_t offset{};
        std::uint64_t payload{};
        std::uint64_t internal_padding{};
        std::uint64_t record_alignment{1};
        bool complete{true};
        result.members.reserve(record->members.size());
        for (auto const& member : record->members) {
            auto member_result{RecordMemberAnalysis{.name = member.name,
                                                    .semantic_type = member.semantic_type.type,
                                                    .element_count = member.count.value_or(1),
                                                    .element_facts = std::nullopt,
                                                    .offset_bytes = std::nullopt,
                                                    .extent_bytes = std::nullopt,
                                                    .padding_before_bytes = std::nullopt}};
            auto const context{"Record '" + node.identity.name + "' member '" + member.name + "'"};
            member_result.element_facts =
                facts_for(member.semantic_type.type, active, result.diagnostics, context);
            if (!member_result.element_facts.has_value()) {
                complete = false;
                result.members.push_back(std::move(member_result));
                continue;
            }
            auto const alignment{member_result.element_facts->alignment_bytes};
            if (member_result.element_facts->size_bytes == 0 || alignment == 0) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " has zero-sized target facts."});
                result.members.push_back(std::move(member_result));
                continue;
            }
            member_result.extent_bytes = checked_multiply(member_result.element_facts->size_bytes,
                                                          member_result.element_count);
            if (!member_result.extent_bytes.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
                result.members.push_back(std::move(member_result));
                continue;
            }
            if (!complete) {
                result.members.push_back(std::move(member_result));
                continue;
            }
            auto const aligned{align_up(offset, alignment)};
            if (!aligned.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " aligned offset overflows uint64."});
                result.members.push_back(std::move(member_result));
                continue;
            }
            member_result.offset_bytes = *aligned;
            member_result.padding_before_bytes = *aligned - offset;
            auto const next_offset{checked_add(*aligned, *member_result.extent_bytes)};
            auto const next_payload{checked_add(payload, *member_result.extent_bytes)};
            auto const next_padding{
                checked_add(internal_padding, *member_result.padding_before_bytes)};
            if (!next_offset.has_value() || !next_payload.has_value() ||
                !next_padding.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " layout arithmetic overflows uint64."});
            } else {
                offset = *next_offset;
                payload = *next_payload;
                internal_padding = *next_padding;
                record_alignment = std::max(record_alignment, alignment);
            }
            result.members.push_back(std::move(member_result));
        }

        if (complete) {
            auto const size{align_up(offset, record_alignment)};
            if (!size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Record tail alignment overflows uint64."});
            } else {
                result.payload_bytes = payload;
                result.internal_padding_bytes = internal_padding;
                result.tail_padding_bytes = *size - offset;
                result.size_bytes = *size;
                result.alignment_bytes = record_alignment;
            }
        }
        active.pop_back();
        return result;
    }

    auto analyze_union(lispb::schema::TypeId const type, std::vector<lispb::schema::TypeId>& active)
        -> UnionAnalysis {
        UnionAnalysis result{.type = type,
                             .alternatives = {},
                             .largest_alternative_bytes = std::nullopt,
                             .tail_padding_bytes = std::nullopt,
                             .size_bytes = std::nullopt,
                             .alignment_bytes = std::nullopt,
                             .diagnostics = {},
                             .aggregate = {}};
        auto const& node{types_.type(type)};
        auto const* union_type{std::get_if<lispb::schema::UnionType>(&node.definition)};
        if (union_type == nullptr) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected semantic type is not a union."});
            return result;
        }
        if (std::ranges::find(active, type) != active.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Illegal by-value aggregate cycle involving '" + node.identity.name + "'."});
            return result;
        }
        active.push_back(type);

        std::uint64_t largest_extent{};
        std::uint64_t union_alignment{1};
        bool complete{true};
        result.alternatives.reserve(union_type->alternatives.size());
        for (auto const& alternative : union_type->alternatives) {
            auto alternative_result{
                UnionAlternativeAnalysis{.name = alternative.name,
                                         .semantic_type = alternative.semantic_type.type,
                                         .element_count = alternative.count.value_or(1),
                                         .element_facts = std::nullopt,
                                         .extent_bytes = std::nullopt,
                                         .slack_bytes = std::nullopt,
                                         .total_slack_bytes = std::nullopt}};
            auto const context{"Union '" + node.identity.name + "' alternative '" +
                               alternative.name + "'"};
            alternative_result.element_facts =
                facts_for(alternative.semantic_type.type, active, result.diagnostics, context);
            if (!alternative_result.element_facts.has_value()) {
                complete = false;
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            auto const alignment{alternative_result.element_facts->alignment_bytes};
            if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " has zero-sized target facts."});
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            alternative_result.extent_bytes = checked_multiply(
                alternative_result.element_facts->size_bytes, alternative_result.element_count);
            if (!alternative_result.extent_bytes.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
            union_alignment = std::max(union_alignment, alignment);
            result.alternatives.push_back(std::move(alternative_result));
        }

        if (complete) {
            auto const size{align_up(largest_extent, union_alignment)};
            if (!size.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Union tail alignment overflows uint64."});
            } else {
                result.largest_alternative_bytes = largest_extent;
                result.tail_padding_bytes = *size - largest_extent;
                result.size_bytes = *size;
                result.alignment_bytes = union_alignment;
                for (auto& alternative : result.alternatives) {
                    alternative.slack_bytes = *size - *alternative.extent_bytes;
                }
            }
        }
        active.pop_back();
        return result;
    }

    auto analyze_tagged_union(lispb::schema::TypeId const type,
                              std::vector<lispb::schema::TypeId>& active) -> TaggedUnionAnalysis {
        TaggedUnionAnalysis result{.type = type,
                                   .discriminant_type = {},
                                   .discriminant_facts = std::nullopt,
                                   .alternatives = {},
                                   .largest_alternative_bytes = std::nullopt,
                                   .payload_size_bytes = std::nullopt,
                                   .payload_alignment_bytes = std::nullopt,
                                   .payload_offset_bytes = std::nullopt,
                                   .internal_padding_bytes = std::nullopt,
                                   .tail_padding_bytes = std::nullopt,
                                   .size_bytes = std::nullopt,
                                   .alignment_bytes = std::nullopt,
                                   .mapped_live_tags = {},
                                   .unmapped_live_tags = {},
                                   .sentinel_tags = {},
                                   .count_sentinel_tag = std::nullopt,
                                   .diagnostics = {},
                                   .aggregate = {}};
        auto const& node{types_.type(type)};
        auto const* tagged{std::get_if<lispb::schema::TaggedUnionType>(&node.definition)};
        if (tagged == nullptr) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected semantic type is not a tagged union."});
            return result;
        }
        result.discriminant_type = tagged->discriminant.type;
        auto const& discriminant_node{types_.type(tagged->discriminant.type)};
        auto const* enumeration{
            std::get_if<lispb::schema::EnumType>(&discriminant_node.definition)};
        if (enumeration != nullptr) {
            std::set<std::string, std::less<>> mapped;
            for (auto const& alternative : tagged->alternatives) {
                mapped.insert(alternative.tag);
            }
            for (auto const& enumerator : enumeration->enumerators) {
                if (enumerator.count_sentinel) {
                    result.count_sentinel_tag = enumerator.name;
                } else if (enumerator.sentinel) {
                    result.sentinel_tags.push_back(enumerator.name);
                } else if (mapped.contains(enumerator.name)) {
                    result.mapped_live_tags.push_back(enumerator.name);
                } else {
                    result.unmapped_live_tags.push_back(enumerator.name);
                }
            }
        }
        if (std::ranges::find(active, type) != active.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Illegal by-value aggregate cycle involving '" + node.identity.name + "'."});
            return result;
        }
        active.push_back(type);

        result.discriminant_facts =
            facts_for(tagged->discriminant.type,
                      active,
                      result.diagnostics,
                      "Tagged union '" + node.identity.name + "' discriminant");
        std::uint64_t largest_extent{};
        std::uint64_t payload_alignment{1};
        bool payload_complete{true};
        result.alternatives.reserve(tagged->alternatives.size());
        for (auto const& alternative : tagged->alternatives) {
            auto alternative_result{
                TaggedUnionAlternativeAnalysis{.name = alternative.name,
                                               .tag = alternative.tag,
                                               .semantic_type = alternative.semantic_type.type,
                                               .element_count = alternative.count.value_or(1),
                                               .element_facts = std::nullopt,
                                               .extent_bytes = std::nullopt,
                                               .payload_slack_bytes = std::nullopt,
                                               .total_payload_slack_bytes = std::nullopt}};
            auto const context{"Tagged union '" + node.identity.name + "' alternative '" +
                               alternative.name + "'"};
            alternative_result.element_facts =
                facts_for(alternative.semantic_type.type, active, result.diagnostics, context);
            if (!alternative_result.element_facts.has_value()) {
                payload_complete = false;
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            auto const alignment{alternative_result.element_facts->alignment_bytes};
            if (alternative_result.element_facts->size_bytes == 0 || alignment == 0) {
                payload_complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " has zero-sized target facts."});
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            alternative_result.extent_bytes = checked_multiply(
                alternative_result.element_facts->size_bytes, alternative_result.element_count);
            if (!alternative_result.extent_bytes.has_value()) {
                payload_complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, context + " fixed-array extent overflows uint64."});
                result.alternatives.push_back(std::move(alternative_result));
                continue;
            }
            largest_extent = std::max(largest_extent, *alternative_result.extent_bytes);
            payload_alignment = std::max(payload_alignment, alignment);
            result.alternatives.push_back(std::move(alternative_result));
        }

        if (payload_complete) {
            auto const payload_size{align_up(largest_extent, payload_alignment)};
            if (!payload_size.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Tagged union payload alignment overflows uint64."});
            } else {
                result.largest_alternative_bytes = largest_extent;
                result.payload_size_bytes = *payload_size;
                result.payload_alignment_bytes = payload_alignment;
                for (auto& alternative : result.alternatives) {
                    alternative.payload_slack_bytes = *payload_size - *alternative.extent_bytes;
                }
            }
        }

        if (result.discriminant_facts.has_value() && result.payload_size_bytes.has_value()) {
            auto const& discriminant{*result.discriminant_facts};
            if (discriminant.size_bytes == 0 || discriminant.alignment_bytes == 0) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged union discriminant has zero-sized target facts."});
            } else {
                auto const payload_offset{
                    align_up(discriminant.size_bytes, *result.payload_alignment_bytes)};
                auto const content_end{
                    payload_offset.has_value()
                        ? checked_add(*payload_offset, *result.payload_size_bytes)
                        : std::nullopt};
                auto const alignment{
                    std::max(discriminant.alignment_bytes, *result.payload_alignment_bytes)};
                auto const size{content_end.has_value() ? align_up(*content_end, alignment)
                                                        : std::nullopt};
                if (!payload_offset.has_value() || !content_end.has_value() || !size.has_value()) {
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         "Tagged union object layout arithmetic overflows uint64."});
                } else {
                    result.payload_offset_bytes = *payload_offset;
                    result.internal_padding_bytes = *payload_offset - discriminant.size_bytes;
                    result.tail_padding_bytes = *size - *content_end;
                    result.size_bytes = *size;
                    result.alignment_bytes = alignment;
                }
            }
        }
        active.pop_back();
        return result;
    }

    lispb::schema::TypeGraph const& types_;
    AbiProfile const& abi_;
};

auto exact_code_count_delta(ExactCodeCount const baseline, ExactCodeCount const variant)
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

auto unsigned_varint_bytes(std::uint64_t value) -> std::uint32_t {
    auto bytes{std::uint32_t{1}};
    while (value >= 0x80) {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

auto signed_integer_value(codegen::PackedIntegerValue const value) -> std::int64_t {
    if (!value.negative) {
        return static_cast<std::int64_t>(value.magnitude);
    }
    if (value.magnitude == (std::uint64_t{1} << 63)) {
        return (std::numeric_limits<std::int64_t>::min)();
    }
    return -static_cast<std::int64_t>(value.magnitude);
}

auto signed_varint_bytes(std::int64_t value) -> std::uint32_t {
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

auto zigzag_value(codegen::PackedIntegerValue const value) -> std::uint64_t {
    return value.negative ? value.magnitude * 2 - 1 : value.magnitude * 2;
}

auto integer_varint_bytes(codegen::PackedIntegerValue const value,
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

auto numeric_delta(std::optional<std::uint64_t> const baseline,
                   std::optional<std::uint64_t> const variant) -> std::optional<NumericDelta> {
    if (!baseline.has_value() || !variant.has_value()) {
        return std::nullopt;
    }
    if (*baseline == *variant) {
        return NumericDelta{.direction = NumericDeltaDirection::unchanged,
                            .magnitude = 0,
                            .percentage =
                                *baseline == 0 ? std::nullopt : std::optional<double>{0.0}};
    }
    auto const increased{*variant > *baseline};
    auto const magnitude{increased ? *variant - *baseline : *baseline - *variant};
    return NumericDelta{.direction = increased ? NumericDeltaDirection::increased
                                               : NumericDeltaDirection::decreased,
                        .magnitude = magnitude,
                        .percentage = *baseline == 0 ? std::nullopt
                                                     : std::optional<double>{
                                                           static_cast<double>(magnitude) * 100.0 /
                                                           static_cast<double>(*baseline)}};
}

auto Analyzer::analyze_enum(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            AbiProfile const& abi,
                            std::uint64_t const element_count) -> EnumDomainAnalysis {
    auto const& enumeration{std::get<lispb::schema::EnumType>(types.type(type).definition)};
    auto const backing_type{enumeration.underlying_type.has_value()
                                ? physical_type_spelling(types, enumeration.underlying_type->type)
                                      .value_or(enumeration.underlying_type->cpp_type.spelling)
                                : derived_enum_backing_type(enumeration).value_or("Unknown")};
    EnumDomainAnalysis result{.type = type,
                              .backing_type = backing_type,
                              .backing_facts = abi.find(backing_type),
                              .backing_bits = std::nullopt,
                              .live_value_count = 0,
                              .reserved_value_count = 0,
                              .minimum_value = std::nullopt,
                              .maximum_value = std::nullopt,
                              .signed_domain = std::nullopt,
                              .declared_signedness = enumeration.signedness,
                              .minimum_required_bits = std::nullopt,
                              .declared_bit_width = enumeration.bit_width,
                              .effective_bit_width = std::nullopt,
                              .semantic_width_can_represent_domain = std::nullopt,
                              .unused_semantic_codes = std::nullopt,
                              .backing_can_represent_domain = std::nullopt,
                              .unused_backing_codes = std::nullopt,
                              .enumerators = {},
                              .diagnostics = {},
                              .aggregate = {.element_count = element_count,
                                            .total_storage_bytes = std::nullopt,
                                            .cache_line_bytes = std::nullopt,
                                            .minimum_cache_lines = std::nullopt,
                                            .complete_elements_per_cache_line = std::nullopt,
                                            .cache_line_straddling_elements = std::nullopt,
                                            .page_bytes = std::nullopt,
                                            .minimum_pages = std::nullopt,
                                            .complete_elements_per_page = std::nullopt,
                                            .page_straddling_elements = std::nullopt,
                                            .cache_capacity = {}}};
    if (backing_type == "Unknown") {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Enum C++ backing storage cannot be derived because semantic signedness or width is "
             "unknown."});
    } else if (!result.backing_facts.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Unknown physical facts for enum backing type '" + backing_type + "'."});
    } else {
        result.backing_bits = checked_multiply(result.backing_facts->size_bytes, 8);
        if (!result.backing_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Enum backing bit count overflows uint64."});
        } else if (*result.backing_bits == 0) {
            result.backing_bits.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Enum backing type has a zero-byte physical size."});
        }
    }

    auto const domain{resolved_enum_domain(enumeration)};
    result.live_value_count = domain.live_value_count;
    result.reserved_value_count = domain.reserved_value_count;
    result.minimum_value = domain.minimum_value;
    result.maximum_value = domain.maximum_value;
    result.signed_domain = domain.signed_domain;
    result.minimum_required_bits = domain.minimum_required_bits;
    result.effective_bit_width =
        enumeration.bit_width.has_value() ? enumeration.bit_width : domain.minimum_required_bits;
    if (domain.minimum_required_bits.has_value() && result.effective_bit_width.has_value()) {
        result.semantic_width_can_represent_domain =
            *result.effective_bit_width >= *domain.minimum_required_bits;
    }
    if (result.effective_bit_width.has_value() && *result.effective_bit_width < 64) {
        auto const semantic_codes{std::uint64_t{1} << *result.effective_bit_width};
        if (domain.distinct_code_count <= semantic_codes) {
            result.unused_semantic_codes = semantic_codes - domain.distinct_code_count;
        }
    }
    result.enumerators.reserve(domain.values.size());
    for (std::size_t index{}; index < domain.values.size(); ++index) {
        auto const& value{domain.values[index]};
        auto const& enumerator{enumeration.enumerators[index]};
        result.enumerators.push_back({.name = value.name,
                                      .sentinel = enumerator.sentinel,
                                      .count_sentinel = enumerator.count_sentinel,
                                      .code = value.code});
    }
    for (auto const& issue : domain.issues) {
        result.diagnostics.push_back(
            {.severity = issue.severity == lispb::schema::EnumDomainIssueSeverity::error
                           ? DiagnosticSeverity::error
                           : DiagnosticSeverity::warning,
             .message = issue.message});
    }

    if (result.minimum_value.has_value() && result.maximum_value.has_value() &&
        result.minimum_required_bits.has_value()) {
        if (result.backing_facts.has_value() && result.backing_bits.has_value()) {
            result.backing_can_represent_domain = enum_domain_fits_backing(*result.minimum_value,
                                                                           *result.maximum_value,
                                                                           *result.backing_facts,
                                                                           *result.backing_bits);
            if (!result.backing_can_represent_domain.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Integer range facts are unavailable for enum backing type '" + backing_type +
                         "'; backing fit is unknown."});
            } else if (!*result.backing_can_represent_domain) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Enum value domain " + lispb::schema::format_enum_code(*result.minimum_value) +
                         " .. " + lispb::schema::format_enum_code(*result.maximum_value) +
                         " does not fit backing type '" + backing_type + "'."});
            }
        }
        auto backing_code_bits{std::optional<std::uint64_t>{}};
        if (result.backing_facts.has_value() &&
            result.backing_facts->integer_signed == std::optional{false} &&
            result.backing_facts->unsigned_value_bits.has_value()) {
            backing_code_bits = *result.backing_facts->unsigned_value_bits;
        } else if (result.backing_facts.has_value() &&
                   result.backing_facts->integer_signed == std::optional{true}) {
            backing_code_bits = result.backing_bits;
        }
        if (backing_code_bits.has_value() && *backing_code_bits < 64) {
            auto const backing_codes{std::uint64_t{1} << *backing_code_bits};
            if (domain.distinct_code_count <= backing_codes) {
                result.unused_backing_codes = backing_codes - domain.distinct_code_count;
            }
        }
    }

    auto const backing_size{
        result.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    if (backing_size.has_value() && *backing_size != 0) {
        result.aggregate.total_storage_bytes = checked_multiply(*backing_size, element_count);
        if (!result.aggregate.total_storage_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Enum standalone backing aggregate storage overflows uint64."});
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
                *memory.cache_line_bytes / *backing_size;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*backing_size,
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
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *backing_size;
            result.aggregate.page_straddling_elements =
                straddling_elements(*backing_size,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());
    return result;
}

auto Analyzer::compare_enum_targets(EnumDomainAnalysis const& first,
                                    EnumDomainAnalysis const& second) -> EnumTargetComparison {
    EnumTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target enum: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target enum: " + diagnostic.message});
    }

    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different enums."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses use different element counts."});
        return result;
    }
    if (first.backing_type != second.backing_type ||
        first.live_value_count != second.live_value_count ||
        first.reserved_value_count != second.reserved_value_count ||
        first.minimum_value != second.minimum_value ||
        first.maximum_value != second.maximum_value ||
        first.signed_domain != second.signed_domain ||
        first.declared_signedness != second.declared_signedness ||
        first.minimum_required_bits != second.minimum_required_bits ||
        first.declared_bit_width != second.declared_bit_width ||
        first.effective_bit_width != second.effective_bit_width ||
        first.semantic_width_can_represent_domain != second.semantic_width_can_represent_domain ||
        first.unused_semantic_codes != second.unused_semantic_codes) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses do not describe the same semantic domain and C++ "
             "backing choice."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    std::set<std::string, std::less<>> second_names;
    for (auto const& enumerator : first.enumerators) {
        first_names.insert(enumerator.name);
    }
    for (auto const& enumerator : second.enumerators) {
        second_names.insert(enumerator.name);
    }
    if (first_names.size() != first.enumerators.size() ||
        second_names.size() != second.enumerators.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses do not contain the same unique enumerators."});
        return result;
    }
    for (std::size_t index{}; index < first.enumerators.size(); ++index) {
        auto const& first_enumerator{first.enumerators[index]};
        auto const& second_enumerator{second.enumerators[index]};
        if (first_enumerator.name != second_enumerator.name ||
            first_enumerator.sentinel != second_enumerator.sentinel ||
            first_enumerator.count_sentinel != second_enumerator.count_sentinel ||
            first_enumerator.code != second_enumerator.code) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target enum analyses do not contain the same ordered enumerators, "
                 "resolved codes, and reserved roles."});
            return result;
        }
    }

    auto const first_size{
        first.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const second_size{
        second.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const first_alignment{first.backing_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const second_alignment{second.backing_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const first_value_bits{first.backing_facts.and_then([](TypeFacts const& facts) {
        return facts.unsigned_value_bits.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    })};
    auto const second_value_bits{second.backing_facts.and_then([](TypeFacts const& facts) {
        return facts.unsigned_value_bits.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    })};
    result.backing_size_delta = numeric_delta(first_size, second_size);
    result.backing_alignment_delta = numeric_delta(first_alignment, second_alignment);
    result.backing_value_bit_delta = numeric_delta(first_value_bits, second_value_bits);
    result.backing_bit_delta = numeric_delta(first.backing_bits, second.backing_bits);
    result.unused_backing_code_delta =
        numeric_delta(first.unused_backing_codes, second.unused_backing_codes);
    if (first.backing_can_represent_domain.has_value() &&
        second.backing_can_represent_domain.has_value()) {
        result.backing_fit_changed =
            *first.backing_can_represent_domain != *second.backing_can_represent_domain;
    }
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
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

auto Analyzer::analyze_integer_scalar(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<RelationshipTargetFacts const> const relationship_targets) -> IntegerScalarAnalysis {
    auto const& scalar{std::get<lispb::schema::IntegerScalarType>(types.type(type).definition)};
    auto required_minimum{scalar.minimum_value};
    auto required_maximum{scalar.maximum_value};
    auto sentinel_count{std::uint64_t{0}};
    std::vector<PackedNamedCodeAnalysis> named_codes;
    named_codes.reserve(scalar.named_codes.size());
    for (auto const& code : scalar.named_codes) {
        named_codes.push_back({.name = code.name, .value = code.value, .sentinel = code.sentinel});
        if (code.sentinel) {
            ++sentinel_count;
        }
        if (codegen::packed_integer_less(code.value, required_minimum)) {
            required_minimum = code.value;
        }
        if (codegen::packed_integer_less(required_maximum, code.value)) {
            required_maximum = code.value;
        }
    }

    auto const live_count{packed_range_count(scalar.minimum_value, scalar.maximum_value)};
    auto const required_count{live_count.has_value() ? checked_add(*live_count, sentinel_count)
                                                     : std::nullopt};
    auto unused_codes{std::optional<std::uint64_t>{}};
    if (required_count.has_value()) {
        unused_codes = scalar.bit_width == 64
                         ? std::optional<std::uint64_t>{std::numeric_limits<std::uint64_t>::max() -
                                                        *required_count + 1}
                         : std::optional<std::uint64_t>{(std::uint64_t{1} << scalar.bit_width) -
                                                        *required_count};
    } else if (scalar.bit_width == 64 && live_count.has_value() &&
               sentinel_count > (std::numeric_limits<std::uint64_t>::max)() - *live_count) {
        unused_codes = 0;
    } else if (sentinel_count == 0 && scalar.bit_width == 64) {
        auto const full_unsigned_domain{
            !scalar.signedness && scalar.minimum_value == codegen::PackedIntegerValue{0} &&
            scalar.maximum_value ==
                codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()}};
        auto const full_signed_domain{
            scalar.signedness &&
            scalar.minimum_value ==
                codegen::PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63) &&
            scalar.maximum_value ==
                codegen::PackedIntegerValue{(std::numeric_limits<std::int64_t>::max)()}};
        if (full_unsigned_domain || full_signed_domain) {
            unused_codes = 0;
        }
    }

    auto result{IntegerScalarAnalysis{
        .type = type,
        .signedness = scalar.signedness,
        .minimum_value = scalar.minimum_value,
        .maximum_value = scalar.maximum_value,
        .live_value_count = live_count,
        .sentinel_code_count = sentinel_count,
        .required_code_count = required_count,
        .minimum_required_bits = *codegen::minimum_packed_integer_bits(
            required_minimum, required_maximum, scalar.signedness),
        .declared_bit_width =
            scalar.bit_width_auto ? std::nullopt : std::optional{scalar.bit_width},
        .effective_bit_width = scalar.bit_width,
        .unused_codes = unused_codes,
        .named_codes = std::move(named_codes),
        .relationship_kind = scalar.relationship.has_value()
                               ? std::optional{scalar.relationship->kind}
                               : std::nullopt,
        .relationship_unit =
            scalar.relationship.has_value() ? scalar.relationship->unit : std::nullopt,
        .relationship_target =
            scalar.relationship.has_value()
                ? std::optional{types.type(scalar.relationship->target.type).identity.name}
                : std::nullopt,
        .relationship_target_extent = std::nullopt,
        .relationship_live_value_count = std::nullopt,
        .relationship_required_code_count = std::nullopt,
        .relationship_minimum_required_bits = std::nullopt,
        .relationship_width_sufficient = std::nullopt,
        .relationship_code_space_capacity_limit = std::nullopt,
        .relationship_capacity_headroom = std::nullopt,
        .relationship_semantic_capacity_limit = std::nullopt,
        .relationship_sentinel_capacity_limit = std::nullopt,
        .relationship_effective_capacity_limit = std::nullopt,
        .relationship_effective_capacity_headroom = std::nullopt,
        .diagnostics = {}}};
    if (scalar.relationship.has_value() &&
        (scalar.relationship->kind == codegen::SemanticRelationKind::index_into ||
         scalar.relationship->kind == codegen::SemanticRelationKind::count_of ||
         scalar.relationship->kind == codegen::SemanticRelationKind::offset_into)) {
        auto const target_facts{std::ranges::find(relationship_targets,
                                                  scalar.relationship->target.type,
                                                  &RelationshipTargetFacts::target)};
        if (target_facts != relationship_targets.end()) {
            auto const target_extent{
                relationship_target_extent(*scalar.relationship, *target_facts)};
            if (!target_extent.has_value()) {
                return result;
            }
            auto const extent_analysis{analyze_unsigned_relationship_extent(
                "Integer scalar '" + types.type(type).identity.name + "'",
                scalar.relationship->kind,
                *target_extent,
                scalar.named_codes,
                scalar.bit_width,
                scalar.minimum_value,
                scalar.maximum_value)};
            result.relationship_target_extent = extent_analysis.target_extent;
            result.relationship_live_value_count = extent_analysis.live_values;
            result.relationship_required_code_count = extent_analysis.required_codes;
            result.relationship_minimum_required_bits = extent_analysis.minimum_required_bits;
            result.relationship_width_sufficient = extent_analysis.width_sufficient;
            result.relationship_code_space_capacity_limit =
                extent_analysis.code_space_capacity_limit;
            result.relationship_capacity_headroom = extent_analysis.capacity_headroom;
            result.relationship_semantic_capacity_limit = extent_analysis.semantic_capacity_limit;
            result.relationship_sentinel_capacity_limit = extent_analysis.sentinel_capacity_limit;
            result.relationship_effective_capacity_limit = extent_analysis.effective_capacity_limit;
            result.relationship_effective_capacity_headroom =
                extent_analysis.effective_capacity_headroom;
            result.diagnostics = extent_analysis.diagnostics;
        }
    }
    return result;
}

auto Analyzer::compare_integer_scalar_capacity(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<RelationshipTargetFacts const> const first_relationship_targets,
    std::span<RelationshipTargetFacts const> const second_relationship_targets)
    -> IntegerScalarCapacityComparison {
    auto first{analyze_integer_scalar(types, type, first_relationship_targets)};
    auto second{analyze_integer_scalar(types, type, second_relationship_targets)};
    auto const widen{[](std::optional<std::uint32_t> const value) {
        return value.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    }};
    auto fit_changed{std::optional<bool>{}};
    if (first.relationship_width_sufficient.has_value() &&
        second.relationship_width_sufficient.has_value()) {
        fit_changed = *first.relationship_width_sufficient != *second.relationship_width_sufficient;
    }
    auto const target_extent_delta{
        numeric_delta(first.relationship_target_extent, second.relationship_target_extent)};
    auto const minimum_width_delta{numeric_delta(widen(first.relationship_minimum_required_bits),
                                                 widen(second.relationship_minimum_required_bits))};
    auto const capacity_limit_delta{numeric_delta(first.relationship_code_space_capacity_limit,
                                                  second.relationship_code_space_capacity_limit)};
    auto const capacity_headroom_delta{
        numeric_delta(first.relationship_capacity_headroom, second.relationship_capacity_headroom)};
    auto const semantic_limit_delta{numeric_delta(first.relationship_semantic_capacity_limit,
                                                  second.relationship_semantic_capacity_limit)};
    auto const sentinel_limit_delta{numeric_delta(first.relationship_sentinel_capacity_limit,
                                                  second.relationship_sentinel_capacity_limit)};
    auto const effective_limit_delta{numeric_delta(first.relationship_effective_capacity_limit,
                                                   second.relationship_effective_capacity_limit)};
    auto const effective_headroom_delta{
        numeric_delta(first.relationship_effective_capacity_headroom,
                      second.relationship_effective_capacity_headroom)};

    return {.first = std::move(first),
            .second = std::move(second),
            .relationship_target_extent_delta = target_extent_delta,
            .relationship_minimum_required_bit_delta = minimum_width_delta,
            .relationship_code_space_capacity_limit_delta = capacity_limit_delta,
            .relationship_capacity_headroom_delta = capacity_headroom_delta,
            .relationship_semantic_capacity_limit_delta = semantic_limit_delta,
            .relationship_sentinel_capacity_limit_delta = sentinel_limit_delta,
            .relationship_effective_capacity_limit_delta = effective_limit_delta,
            .relationship_effective_capacity_headroom_delta = effective_headroom_delta,
            .relationship_width_fit_changed = fit_changed};
}

auto Analyzer::analyze_optional_sentinel(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId const type,
                                         std::uint64_t const element_count)
    -> OptionalSentinelAnalysis {
    auto const& optional{
        std::get<lispb::schema::OptionalSentinelType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(optional.source.type).definition)};
    auto const source_analysis{analyze_integer_scalar(types, optional.source.type)};

    OptionalSentinelAnalysis result{
        .type = type,
        .source_type = optional.source.type,
        .sentinel_name = optional.sentinel_name,
        .sentinel_value = optional.sentinel_value,
        .source_signedness = source.signedness,
        .source_minimum = source.minimum_value,
        .source_maximum = source.maximum_value,
        .present_value_count = source_analysis.live_value_count,
        .absence_code_count = 1,
        .other_sentinel_code_count = source_analysis.sentinel_code_count - 1,
        .unused_code_count = source_analysis.unused_codes,
        .encoded_storage_bits = optional.bit_width,
        .total_code_count = optional.bit_width == 64
                              ? ExactCodeCount{.value = 0, .two_to_64 = true}
                              : ExactCodeCount{.value = std::uint64_t{1} << optional.bit_width,
                                               .two_to_64 = false},
        .element_count = element_count,
        .total_encoded_bits = checked_multiply(optional.bit_width, element_count),
        .diagnostics = {},
    };
    if (!result.total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Optional sentinel aggregate encoded-bit total exceeds the supported 64-bit range."});
    }
    return result;
}

auto Analyzer::analyze_optional_presence_bit(lispb::schema::TypeGraph const& types,
                                             lispb::schema::TypeId const type,
                                             std::uint64_t const element_count)
    -> OptionalPresenceBitAnalysis {
    auto const& optional{
        std::get<lispb::schema::OptionalPresenceBitType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(optional.source.type).definition)};
    auto const source_analysis{analyze_integer_scalar(types, optional.source.type)};

    OptionalPresenceBitAnalysis result{
        .type = type,
        .source_type = optional.source.type,
        .source_signedness = source.signedness,
        .source_minimum = source.minimum_value,
        .source_maximum = source.maximum_value,
        .present_value_count = source_analysis.live_value_count,
        .canonical_absence_state_count = 1,
        .source_sentinel_code_count = source_analysis.sentinel_code_count,
        .source_unused_payload_codes = source_analysis.unused_codes,
        .presence_bits = 1,
        .payload_bits = optional.payload_bits,
        .encoded_storage_bits = optional.encoded_bits,
        .noncanonical_absence_patterns = optional.payload_bits == 64
                                           ? (std::numeric_limits<std::uint64_t>::max)()
                                           : (std::uint64_t{1} << optional.payload_bits) - 1,
        .element_count = element_count,
        .total_encoded_bits = checked_multiply(optional.encoded_bits, element_count),
        .diagnostics = {},
    };
    if (!result.total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Optional presence-bit aggregate encoded-bit total exceeds the supported 64-bit "
             "range."});
    }
    return result;
}

auto Analyzer::compare_optional_encodings(lispb::schema::TypeGraph const& types,
                                          lispb::schema::TypeId const first,
                                          lispb::schema::TypeId const second,
                                          std::uint64_t const element_count)
    -> OptionalEncodingComparison {
    OptionalEncodingComparison result{};
    result.element_count = element_count;

    auto summarize =
        [&](lispb::schema::TypeId const type) -> std::optional<OptionalEncodingSummary> {
        auto const& definition{types.type(type).definition};
        if (std::holds_alternative<lispb::schema::OptionalSentinelType>(definition)) {
            auto const analysis{analyze_optional_sentinel(types, type, element_count)};
            result.diagnostics.insert(
                result.diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
            return OptionalEncodingSummary{
                .type = type,
                .source_type = analysis.source_type,
                .kind = OptionalEncodingKind::sentinel,
                .absence_sentinel_name = analysis.sentinel_name,
                .present_value_count = analysis.present_value_count,
                .canonical_absence_state_count = analysis.absence_code_count,
                .source_sentinel_code_count = analysis.other_sentinel_code_count + 1,
                .source_sentinel_codes_used_for_absence = 1,
                .remaining_source_sentinel_code_count = analysis.other_sentinel_code_count,
                .source_unused_payload_codes = analysis.unused_code_count,
                .noncanonical_absence_patterns = 0,
                .encoded_storage_bits = analysis.encoded_storage_bits,
                .total_encoded_bits = analysis.total_encoded_bits,
            };
        }
        if (std::holds_alternative<lispb::schema::OptionalPresenceBitType>(definition)) {
            auto const analysis{analyze_optional_presence_bit(types, type, element_count)};
            result.diagnostics.insert(
                result.diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
            return OptionalEncodingSummary{
                .type = type,
                .source_type = analysis.source_type,
                .kind = OptionalEncodingKind::presence_bit,
                .absence_sentinel_name = std::nullopt,
                .present_value_count = analysis.present_value_count,
                .canonical_absence_state_count = analysis.canonical_absence_state_count,
                .source_sentinel_code_count = analysis.source_sentinel_code_count,
                .source_sentinel_codes_used_for_absence = 0,
                .remaining_source_sentinel_code_count = analysis.source_sentinel_code_count,
                .source_unused_payload_codes = analysis.source_unused_payload_codes,
                .noncanonical_absence_patterns = analysis.noncanonical_absence_patterns,
                .encoded_storage_bits = analysis.encoded_storage_bits,
                .total_encoded_bits = analysis.total_encoded_bits,
            };
        }
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Type '" + types.type(type).identity.name +
                 "' is not a sentinel or presence-bit optional representation."});
        return std::nullopt;
    };

    auto const first_summary{summarize(first)};
    auto const second_summary{summarize(second)};
    result.supported_encodings = first_summary.has_value() && second_summary.has_value();
    if (!result.supported_encodings) {
        return result;
    }
    result.first = *first_summary;
    result.second = *second_summary;
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Optional representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.encoded_storage_bit_delta =
        numeric_delta(result.first.encoded_storage_bits, result.second.encoded_storage_bits);
    result.total_encoded_bit_delta =
        numeric_delta(result.first.total_encoded_bits, result.second.total_encoded_bits);
    return result;
}

auto Analyzer::analyze_linear_quantized(lispb::schema::TypeGraph const& types,
                                        lispb::schema::TypeId const type)
    -> LinearQuantizedAnalysis {
    auto const& quantized{
        std::get<lispb::schema::LinearQuantizedType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(quantized.source.type).definition)};

    auto const total_code_count{
        quantized.bit_width == 64
            ? ExactCodeCount{.value = 0, .two_to_64 = true}
            : ExactCodeCount{.value = std::uint64_t{1} << quantized.bit_width, .two_to_64 = false}};
    auto const usable_code_count{
        total_code_count.two_to_64 && quantized.reserved_codes == 0
            ? total_code_count
            : ExactCodeCount{.value = total_code_count.two_to_64
                                        ? (std::numeric_limits<std::uint64_t>::max)() -
                                              quantized.reserved_codes + 1
                                        : total_code_count.value - quantized.reserved_codes,
                             .two_to_64 = false}};

    auto const source_span{packed_range_span(source.minimum_value, source.maximum_value)};
    auto const numerical_code_count{std::ldexp(1.0L, static_cast<int>(quantized.bit_width))};
    auto const numerical_usable_count{numerical_code_count -
                                      static_cast<long double>(quantized.reserved_codes)};
    auto const numerical_span{source_span.has_value()
                                  ? static_cast<long double>(*source_span)
                                  : packed_integer_as_long_double(source.maximum_value) -
                                        packed_integer_as_long_double(source.minimum_value)};
    auto const resolution{numerical_span / (numerical_usable_count - 1.0L)};

    return {.type = type,
            .source_type = quantized.source.type,
            .source_minimum = source.minimum_value,
            .source_maximum = source.maximum_value,
            .source_span = source_span,
            .encoded_storage_bits = quantized.bit_width,
            .total_code_count = total_code_count,
            .reserved_code_count = quantized.reserved_codes,
            .usable_code_count = usable_code_count,
            .resolution = resolution,
            .maximum_rounding_error = resolution / 2.0L,
            .minimum_endpoint_exact = true,
            .maximum_endpoint_exact = true,
            .clipping = quantized.clipping};
}

auto Analyzer::compare_linear_quantized(lispb::schema::TypeGraph const& types,
                                        lispb::schema::TypeId const first,
                                        lispb::schema::TypeId const second,
                                        std::uint64_t const element_count)
    -> LinearQuantizedComparison {
    LinearQuantizedComparison result{};
    result.first = analyze_linear_quantized(types, first);
    result.second = analyze_linear_quantized(types, second);
    result.element_count = element_count;
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Linear quantized representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.encoded_storage_bit_delta =
        numeric_delta(result.first.encoded_storage_bits, result.second.encoded_storage_bits);
    result.total_code_count_delta =
        exact_code_count_delta(result.first.total_code_count, result.second.total_code_count);
    result.usable_code_count_delta =
        exact_code_count_delta(result.first.usable_code_count, result.second.usable_code_count);
    result.reserved_code_count_delta =
        numeric_delta(result.first.reserved_code_count, result.second.reserved_code_count);
    result.clipping_changed = result.first.clipping != result.second.clipping;
    result.resolution_delta = result.second.resolution - result.first.resolution;
    result.maximum_rounding_error_delta =
        result.second.maximum_rounding_error - result.first.maximum_rounding_error;
    result.first_total_encoded_bits =
        checked_multiply(result.first.encoded_storage_bits, element_count);
    result.second_total_encoded_bits =
        checked_multiply(result.second.encoded_storage_bits, element_count);
    if (!result.first_total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "First encoded payload bit count overflows uint64."});
    }
    if (!result.second_total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Second encoded payload bit count overflows uint64."});
    }
    result.total_encoded_bit_delta =
        numeric_delta(result.first_total_encoded_bits, result.second_total_encoded_bits);
    return result;
}

auto Analyzer::analyze_fixed_point(lispb::schema::TypeGraph const& types,
                                   lispb::schema::TypeId const type,
                                   std::uint64_t const element_count) -> FixedPointAnalysis {
    auto const& fixed{std::get<lispb::schema::FixedPointType>(types.type(type).definition)};
    auto const sign_bits{fixed.signedness ? std::uint32_t{1} : std::uint32_t{0}};
    auto const whole_bits{fixed.total_bits - fixed.fractional_bits - sign_bits};
    auto const resolution{std::ldexp(1.0L, -static_cast<int>(fixed.fractional_bits))};
    auto const scale{std::ldexp(1.0L, static_cast<int>(fixed.fractional_bits))};

    codegen::PackedIntegerValue minimum_raw;
    codegen::PackedIntegerValue maximum_raw;
    long double minimum_value{};
    long double maximum_value{};
    if (fixed.signedness) {
        auto const negative_magnitude{std::uint64_t{1} << (fixed.total_bits - 1)};
        minimum_raw = codegen::PackedIntegerValue::from_parts(true, negative_magnitude);
        maximum_raw = codegen::PackedIntegerValue{negative_magnitude - 1};
        auto const range_limit{std::ldexp(1.0L,
                                          static_cast<int>(fixed.total_bits - 1) -
                                              static_cast<int>(fixed.fractional_bits))};
        minimum_value = -range_limit;
        maximum_value = range_limit - resolution;
    } else {
        minimum_raw = codegen::PackedIntegerValue{0};
        maximum_raw = codegen::PackedIntegerValue{fixed.total_bits == 64
                                                      ? (std::numeric_limits<std::uint64_t>::max)()
                                                      : (std::uint64_t{1} << fixed.total_bits) - 1};
        maximum_value = std::ldexp(1.0L,
                                   static_cast<int>(fixed.total_bits) -
                                       static_cast<int>(fixed.fractional_bits)) -
                        resolution;
    }

    auto const total_encoded_bits{checked_multiply(fixed.total_bits, element_count)};
    FixedPointAnalysis result{.type = type,
                              .signedness = fixed.signedness,
                              .total_bits = fixed.total_bits,
                              .fractional_bits = fixed.fractional_bits,
                              .whole_bits = whole_bits,
                              .minimum_raw_value = minimum_raw,
                              .maximum_raw_value = maximum_raw,
                              .scale = scale,
                              .resolution = resolution,
                              .minimum_value = minimum_value,
                              .maximum_value = maximum_value,
                              .maximum_rounding_error =
                                  fixed.rounding == codegen::FixedPointRounding::nearest_even
                                      ? resolution / 2.0L
                                      : resolution,
                              .rounding = fixed.rounding,
                              .element_count = element_count,
                              .total_encoded_bits = total_encoded_bits,
                              .diagnostics = {}};
    if (!total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Fixed-point encoded payload bit count overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_mini_float(lispb::schema::TypeGraph const& types,
                                  lispb::schema::TypeId const type,
                                  std::uint64_t const element_count) -> MiniFloatAnalysis {
    auto const& mini{std::get<lispb::schema::MiniFloatType>(types.type(type).definition)};
    auto const total_bits{mini.sign_bits + mini.exponent_bits + mini.significand_bits};
    auto const exponent_code_count{std::uint64_t{1} << mini.exponent_bits};
    auto const normal_exponent_code_count{exponent_code_count - 2};
    auto const sign_code_count{mini.sign_bits == 0 ? std::uint64_t{1} : std::uint64_t{2}};
    auto const significand_code_count{std::uint64_t{1} << mini.significand_bits};
    auto const minimum_normal_exponent{std::int32_t{1} - mini.exponent_bias};
    auto const maximum_normal_exponent{static_cast<std::int32_t>(exponent_code_count - 2) -
                                       mini.exponent_bias};
    auto const total_encoded_bits{checked_multiply(total_bits, element_count)};

    MiniFloatAnalysis result{
        .type = type,
        .sign_bits = mini.sign_bits,
        .exponent_bits = mini.exponent_bits,
        .significand_bits = mini.significand_bits,
        .total_bits = total_bits,
        .exponent_bias = mini.exponent_bias,
        .exponent_code_count = exponent_code_count,
        .normal_exponent_code_count = normal_exponent_code_count,
        .minimum_normal_exponent = minimum_normal_exponent,
        .maximum_normal_exponent = maximum_normal_exponent,
        .total_code_count = {.value = total_bits == 64 ? 0 : std::uint64_t{1} << total_bits,
                             .two_to_64 = total_bits == 64},
        .zero_code_count = sign_code_count,
        .infinity_code_count = sign_code_count,
        .nan_code_count = (significand_code_count - 1) * sign_code_count,
        .nonzero_subnormal_code_count = (significand_code_count - 1) * sign_code_count,
        .minimum_positive_subnormal = std::nullopt,
        .minimum_positive_normal = std::nullopt,
        .maximum_finite = std::nullopt,
        .minimum_finite = std::nullopt,
        .unit_interval_resolution = std::nullopt,
        .maximum_relative_rounding_error =
            std::ldexp(1.0L, -static_cast<int>(mini.significand_bits) - 1),
        .element_count = element_count,
        .total_encoded_bits = total_encoded_bits,
        .diagnostics = {}};

    auto retain_positive_finite =
        [&](long double const value,
            std::string_view const description) -> std::optional<long double> {
        if (std::isfinite(value) && value > 0.0L) {
            return value;
        }
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             std::string{description} +
                 " is outside the host long-double numerical range; exponent facts remain exact."});
        return std::nullopt;
    };

    result.minimum_positive_normal = retain_positive_finite(
        std::ldexp(1.0L, minimum_normal_exponent), "Minimum positive normal value");
    if (mini.significand_bits != 0) {
        result.minimum_positive_subnormal = retain_positive_finite(
            std::ldexp(1.0L, minimum_normal_exponent - static_cast<int>(mini.significand_bits)),
            "Minimum positive subnormal value");
    }
    auto const maximum_significand{2.0L -
                                   std::ldexp(1.0L, -static_cast<int>(mini.significand_bits))};
    result.maximum_finite = retain_positive_finite(
        std::ldexp(maximum_significand, maximum_normal_exponent), "Maximum finite value");
    if (result.maximum_finite.has_value()) {
        result.minimum_finite = mini.sign_bits == 0 ? std::optional<long double>{0.0L}
                                                    : std::optional{-*result.maximum_finite};
    }
    if (mini.exponent_bias >= 1 &&
        mini.exponent_bias <= static_cast<std::int32_t>(exponent_code_count - 2)) {
        result.unit_interval_resolution =
            std::ldexp(1.0L, -static_cast<int>(mini.significand_bits));
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "1.0 is outside the normal exponent range, so unit-interval resolution is unknown."});
    }
    if (!total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Mini-float encoded payload bit count overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_integer_varint(lispb::schema::TypeGraph const& types,
                                      lispb::schema::TypeId const type,
                                      std::uint64_t const element_count) -> IntegerVarintAnalysis {
    auto const& varint{std::get<lispb::schema::IntegerVarintType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(varint.source.type).definition)};

    auto const minimum_endpoint_bytes{integer_varint_bytes(source.minimum_value, varint.encoding)};
    auto const maximum_endpoint_bytes{integer_varint_bytes(source.maximum_value, varint.encoding)};
    auto minimum_bytes{std::min(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    auto const zero{codegen::PackedIntegerValue{0}};
    if (codegen::packed_integer_less_equal(source.minimum_value, zero) &&
        codegen::packed_integer_less_equal(zero, source.maximum_value)) {
        minimum_bytes = 1;
    }
    auto maximum_bytes{std::max(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    for (auto const& code : source.named_codes) {
        auto const bytes{integer_varint_bytes(code.value, varint.encoding)};
        minimum_bytes = std::min(minimum_bytes, bytes);
        maximum_bytes = std::max(maximum_bytes, bytes);
    }

    IntegerVarintAnalysis result{
        .type = type,
        .source_type = varint.source.type,
        .encoding = varint.encoding,
        .minimum_encoded_bytes = minimum_bytes,
        .maximum_encoded_bytes = maximum_bytes,
        .element_count = element_count,
        .minimum_total_bytes = checked_multiply(minimum_bytes, element_count),
        .maximum_total_bytes = checked_multiply(maximum_bytes, element_count),
        .diagnostics = {}};
    if (!result.minimum_total_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Minimum varint byte total overflows uint64."});
    }
    if (!result.maximum_total_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Maximum varint byte total overflows uint64."});
    }
    return result;
}

auto Analyzer::compare_integer_varint(lispb::schema::TypeGraph const& types,
                                      lispb::schema::TypeId const first,
                                      lispb::schema::TypeId const second,
                                      std::uint64_t const element_count)
    -> IntegerVarintComparison {
    IntegerVarintComparison result{};
    result.first = analyze_integer_varint(types, first, element_count);
    result.second = analyze_integer_varint(types, second, element_count);
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Integer varint representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.minimum_encoded_byte_delta =
        numeric_delta(result.first.minimum_encoded_bytes, result.second.minimum_encoded_bytes);
    result.maximum_encoded_byte_delta =
        numeric_delta(result.first.maximum_encoded_bytes, result.second.maximum_encoded_bytes);
    result.minimum_total_byte_delta =
        numeric_delta(result.first.minimum_total_bytes, result.second.minimum_total_bytes);
    result.maximum_total_byte_delta =
        numeric_delta(result.first.maximum_total_bytes, result.second.maximum_total_bytes);
    result.diagnostics = result.first.diagnostics;
    result.diagnostics.insert(result.diagnostics.end(),
                              result.second.diagnostics.begin(),
                              result.second.diagnostics.end());
    return result;
}

auto Analyzer::analyze_integer_varint_distribution(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<IntegerVarintDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> IntegerVarintDistributionAnalysis {
    auto const& varint{std::get<lispb::schema::IntegerVarintType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(varint.source.type).definition)};
    IntegerVarintDistributionAnalysis result{.type = type,
                                             .valid_entry_count = 0,
                                             .total_weight = 0,
                                             .total_encoded_bytes = 0,
                                             .expected_bytes_per_value = std::nullopt,
                                             .selected_element_count = selected_element_count,
                                             .expected_selected_bytes = std::nullopt,
                                             .entries = {},
                                             .diagnostics = {}};

    auto exact_weight_overflow_reported{false};
    auto exact_bytes_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_bytes{0.0L};
    for (auto const& entry : entries) {
        auto const in_live_range{
            codegen::packed_integer_less_equal(source.minimum_value, entry.value) &&
            codegen::packed_integer_less_equal(entry.value, source.maximum_value)};
        auto const named{std::ranges::any_of(
            source.named_codes, [&](auto const& code) { return code.value == entry.value; })};
        if (!in_live_range && !named) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Distribution value " +
                                              codegen::format_packed_integer(entry.value) +
                                              " is outside the semantic source domain."});
            continue;
        }

        ++result.valid_entry_count;
        auto const bytes{integer_varint_bytes(entry.value, varint.encoding)};
        auto const weighted_bytes{checked_multiply(entry.weight, bytes)};
        result.entries.push_back({.value = entry.value,
                                  .weight = entry.weight,
                                  .encoded_bytes = bytes,
                                  .weighted_encoded_bytes = weighted_bytes});
        numerical_weight += static_cast<long double>(entry.weight);
        numerical_bytes += static_cast<long double>(entry.weight) * bytes;

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_encoded_bytes.has_value()) {
            result.total_encoded_bytes =
                weighted_bytes.has_value()
                    ? checked_add(*result.total_encoded_bytes, *weighted_bytes)
                    : std::nullopt;
            if (!result.total_encoded_bytes.has_value() && !exact_bytes_overflow_reported) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Distribution encoded byte total overflows uint64."});
                exact_bytes_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L) {
        result.expected_bytes_per_value = numerical_bytes / numerical_weight;
        result.expected_selected_bytes =
            *result.expected_bytes_per_value * static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected varint size is Unknown because the distribution has no positive valid "
             "weight."});
    }
    return result;
}

auto Analyzer::compare_integer_varint_distribution(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const first,
    lispb::schema::TypeId const second,
    std::span<IntegerVarintDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> IntegerVarintDistributionComparison {
    auto const& first_varint{
        std::get<lispb::schema::IntegerVarintType>(types.type(first).definition)};
    auto const& second_varint{
        std::get<lispb::schema::IntegerVarintType>(types.type(second).definition)};
    IntegerVarintDistributionComparison result{
        .first = analyze_integer_varint_distribution(types, first, entries, selected_element_count),
        .second =
            analyze_integer_varint_distribution(types, second, entries, selected_element_count),
        .compatible_source = first_varint.source.type == second_varint.source.type,
        .total_encoded_byte_delta = std::nullopt,
        .expected_bytes_per_value_delta = std::nullopt,
        .expected_selected_bytes_delta = std::nullopt,
        .diagnostics = {}};
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Integer varint distributions for '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.total_encoded_byte_delta =
        numeric_delta(result.first.total_encoded_bytes, result.second.total_encoded_bytes);
    if (result.first.expected_bytes_per_value.has_value() &&
        result.second.expected_bytes_per_value.has_value()) {
        result.expected_bytes_per_value_delta =
            *result.second.expected_bytes_per_value - *result.first.expected_bytes_per_value;
    }
    if (result.first.expected_selected_bytes.has_value() &&
        result.second.expected_selected_bytes.has_value()) {
        result.expected_selected_bytes_delta =
            *result.second.expected_selected_bytes - *result.first.expected_selected_bytes;
    }
    result.diagnostics = result.first.diagnostics;
    result.diagnostics.insert(result.diagnostics.end(),
                              result.second.diagnostics.begin(),
                              result.second.diagnostics.end());
    return result;
}

auto Analyzer::analyze_record(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              AbiProfile const& abi,
                              std::uint64_t const element_count) -> RecordAnalysis {
    auto result{AggregateLayoutAnalyzer{types, abi}.analyze_record(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Record aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_payload_bytes = scale(result.payload_bytes, "payload byte count");
    result.aggregate.total_internal_padding_bytes =
        scale(result.internal_padding_bytes, "internal-padding byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    if (result.aggregate.total_internal_padding_bytes.has_value() &&
        result.aggregate.total_tail_padding_bytes.has_value()) {
        result.aggregate.total_padding_bytes =
            checked_add(*result.aggregate.total_internal_padding_bytes,
                        *result.aggregate.total_tail_padding_bytes);
        if (!result.aggregate.total_padding_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Record aggregate padding byte count overflows uint64."});
        }
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
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
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
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
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_record_targets(RecordAnalysis const& first, RecordAnalysis const& second)
    -> RecordTargetComparison {
    RecordTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target record: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target record: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different records."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target record analyses use different element counts."});
        return result;
    }

    std::set<std::string, std::less<>> expected_names;
    for (auto const& member : first.members) {
        expected_names.insert(member.name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& member : second.members) {
        second_names.insert(member.name);
    }
    if (expected_names.size() != first.members.size() ||
        second_names.size() != second.members.size() || expected_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target record analyses do not contain the same unique members."});
        return result;
    }

    for (auto const& first_member : first.members) {
        auto const second_member{
            std::ranges::find(second.members, first_member.name, &RecordMemberAnalysis::name)};
        if (second_member == second.members.end() ||
            first_member.semantic_type != second_member->semantic_type ||
            first_member.element_count != second_member->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target record member '" + first_member.name +
                     "' has incompatible semantic identity or element count."});
            return result;
        }
    }

    for (auto const& first_member : first.members) {
        auto const second_member{
            std::ranges::find(second.members, first_member.name, &RecordMemberAnalysis::name)};
        auto const first_size{first_member.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_member->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_member.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_member->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.members.push_back(
            {.name = first_member.name,
             .first = first_member,
             .second = *second_member,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .offset_delta = numeric_delta(first_member.offset_bytes, second_member->offset_bytes),
             .extent_delta = numeric_delta(first_member.extent_bytes, second_member->extent_bytes),
             .padding_before_delta = numeric_delta(first_member.padding_before_bytes,
                                                   second_member->padding_before_bytes)});
    }

    result.payload_delta = numeric_delta(first.payload_bytes, second.payload_bytes);
    result.internal_padding_delta =
        numeric_delta(first.internal_padding_bytes, second.internal_padding_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_payload_delta =
        numeric_delta(first.aggregate.total_payload_bytes, second.aggregate.total_payload_bytes);
    result.total_internal_padding_delta =
        numeric_delta(first.aggregate.total_internal_padding_bytes,
                      second.aggregate.total_internal_padding_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
    result.total_padding_delta =
        numeric_delta(first.aggregate.total_padding_bytes, second.aggregate.total_padding_bytes);
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

auto Analyzer::analyze_union(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId const type,
                             AbiProfile const& abi,
                             std::uint64_t const element_count) -> UnionAnalysis {
    auto result{AggregateLayoutAnalyzer{types, abi}.analyze_union(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Union aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    for (auto& alternative : result.alternatives) {
        alternative.total_slack_bytes =
            scale(alternative.slack_bytes, "alternative '" + alternative.name + "' slack");
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
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
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
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
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_union_targets(UnionAnalysis const& first, UnionAnalysis const& second)
    -> UnionTargetComparison {
    UnionTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target raw union: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target raw union: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different raw unions."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target raw-union analyses use different element counts."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    for (auto const& alternative : first.alternatives) {
        first_names.insert(alternative.name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& alternative : second.alternatives) {
        second_names.insert(alternative.name);
    }
    if (first_names.size() != first.alternatives.size() ||
        second_names.size() != second.alternatives.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target raw-union analyses do not contain the same unique alternatives."});
        return result;
    }

    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &UnionAlternativeAnalysis::name)};
        if (second_alternative == second.alternatives.end() ||
            first_alternative.semantic_type != second_alternative->semantic_type ||
            first_alternative.element_count != second_alternative->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target raw-union alternative '" + first_alternative.name +
                     "' has incompatible semantic identity or element count."});
            return result;
        }
    }

    result.alternatives.reserve(first.alternatives.size());
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &UnionAlternativeAnalysis::name)};
        auto const first_size{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.alternatives.push_back(
            {.name = first_alternative.name,
             .first = first_alternative,
             .second = *second_alternative,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .extent_delta =
                 numeric_delta(first_alternative.extent_bytes, second_alternative->extent_bytes),
             .slack_delta =
                 numeric_delta(first_alternative.slack_bytes, second_alternative->slack_bytes),
             .total_slack_delta = numeric_delta(first_alternative.total_slack_bytes,
                                                second_alternative->total_slack_bytes)});
    }

    result.largest_alternative_delta =
        numeric_delta(first.largest_alternative_bytes, second.largest_alternative_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
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

auto Analyzer::analyze_tagged_union(lispb::schema::TypeGraph const& types,
                                    lispb::schema::TypeId const type,
                                    AbiProfile const& abi,
                                    std::uint64_t const element_count) -> TaggedUnionAnalysis {
    auto result{AggregateLayoutAnalyzer{types, abi}.analyze_tagged_union(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_discriminant_bytes = scale(
        result.discriminant_facts.has_value() ? std::optional{result.discriminant_facts->size_bytes}
                                              : std::nullopt,
        "discriminant byte count");
    result.aggregate.total_payload_bytes = scale(result.payload_size_bytes, "payload byte count");
    result.aggregate.total_internal_padding_bytes =
        scale(result.internal_padding_bytes, "internal-padding byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    if (result.aggregate.total_internal_padding_bytes.has_value() &&
        result.aggregate.total_tail_padding_bytes.has_value()) {
        result.aggregate.total_padding_bytes =
            checked_add(*result.aggregate.total_internal_padding_bytes,
                        *result.aggregate.total_tail_padding_bytes);
        if (!result.aggregate.total_padding_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union aggregate padding byte count overflows uint64."});
        }
    }
    for (auto& alternative : result.alternatives) {
        alternative.total_payload_slack_bytes =
            scale(alternative.payload_slack_bytes,
                  "alternative '" + alternative.name + "' payload slack");
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
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
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
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
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_tagged_union_targets(TaggedUnionAnalysis const& first,
                                            TaggedUnionAnalysis const& second)
    -> TaggedUnionTargetComparison {
    TaggedUnionTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target tagged union: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target tagged union: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target analyses describe different tagged unions."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses use different element counts."});
        return result;
    }
    if (first.discriminant_type != second.discriminant_type ||
        first.mapped_live_tags != second.mapped_live_tags ||
        first.unmapped_live_tags != second.unmapped_live_tags ||
        first.sentinel_tags != second.sentinel_tags ||
        first.count_sentinel_tag != second.count_sentinel_tag) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses have incompatible discriminant or tag "
             "coverage semantics."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    std::set<std::string, std::less<>> first_tags;
    for (auto const& alternative : first.alternatives) {
        first_names.insert(alternative.name);
        first_tags.insert(alternative.tag);
    }
    std::set<std::string, std::less<>> second_names;
    std::set<std::string, std::less<>> second_tags;
    for (auto const& alternative : second.alternatives) {
        second_names.insert(alternative.name);
        second_tags.insert(alternative.tag);
    }
    if (first_names.size() != first.alternatives.size() ||
        first_tags.size() != first.alternatives.size() ||
        second_names.size() != second.alternatives.size() ||
        second_tags.size() != second.alternatives.size() || first_names != second_names ||
        first_tags != second_tags) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses do not contain the same unique alternatives "
             "and tags."});
        return result;
    }
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &TaggedUnionAlternativeAnalysis::name)};
        if (second_alternative == second.alternatives.end() ||
            first_alternative.tag != second_alternative->tag ||
            first_alternative.semantic_type != second_alternative->semantic_type ||
            first_alternative.element_count != second_alternative->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target tagged-union alternative '" + first_alternative.name +
                     "' has incompatible tag, semantic identity, or element count."});
            return result;
        }
    }

    result.alternatives.reserve(first.alternatives.size());
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &TaggedUnionAlternativeAnalysis::name)};
        auto const first_size{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.alternatives.push_back(
            {.name = first_alternative.name,
             .first = first_alternative,
             .second = *second_alternative,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .extent_delta =
                 numeric_delta(first_alternative.extent_bytes, second_alternative->extent_bytes),
             .payload_slack_delta = numeric_delta(first_alternative.payload_slack_bytes,
                                                  second_alternative->payload_slack_bytes),
             .total_payload_slack_delta =
                 numeric_delta(first_alternative.total_payload_slack_bytes,
                               second_alternative->total_payload_slack_bytes)});
    }

    auto const first_discriminant_size{first.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const second_discriminant_size{second.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const first_discriminant_alignment{first.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const second_discriminant_alignment{second.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    result.discriminant_size_delta =
        numeric_delta(first_discriminant_size, second_discriminant_size);
    result.discriminant_alignment_delta =
        numeric_delta(first_discriminant_alignment, second_discriminant_alignment);
    result.largest_alternative_delta =
        numeric_delta(first.largest_alternative_bytes, second.largest_alternative_bytes);
    result.payload_size_delta = numeric_delta(first.payload_size_bytes, second.payload_size_bytes);
    result.payload_alignment_delta =
        numeric_delta(first.payload_alignment_bytes, second.payload_alignment_bytes);
    result.payload_offset_delta =
        numeric_delta(first.payload_offset_bytes, second.payload_offset_bytes);
    result.internal_padding_delta =
        numeric_delta(first.internal_padding_bytes, second.internal_padding_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_discriminant_delta = numeric_delta(first.aggregate.total_discriminant_bytes,
                                                    second.aggregate.total_discriminant_bytes);
    result.total_payload_delta =
        numeric_delta(first.aggregate.total_payload_bytes, second.aggregate.total_payload_bytes);
    result.total_internal_padding_delta =
        numeric_delta(first.aggregate.total_internal_padding_bytes,
                      second.aggregate.total_internal_padding_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
    result.total_padding_delta =
        numeric_delta(first.aggregate.total_padding_bytes, second.aggregate.total_padding_bytes);
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

auto Analyzer::analyze_union_distribution(UnionAnalysis const& analysis,
                                          std::span<UnionDistributionEntry const> const entries,
                                          std::uint64_t const selected_element_count)
    -> UnionDistributionAnalysis {
    UnionDistributionAnalysis result{.type = analysis.type,
                                     .valid_entry_count = 0,
                                     .total_weight = 0,
                                     .total_extent_bytes = 0,
                                     .total_slack_bytes = 0,
                                     .expected_extent_bytes_per_value = std::nullopt,
                                     .expected_slack_bytes_per_value = std::nullopt,
                                     .selected_element_count = selected_element_count,
                                     .expected_selected_extent_bytes = std::nullopt,
                                     .expected_selected_slack_bytes = std::nullopt,
                                     .entries = {},
                                     .diagnostics = {}};
    std::set<std::string, std::less<>> seen_alternatives;
    auto exact_weight_overflow_reported{false};
    auto exact_extent_overflow_reported{false};
    auto exact_slack_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_extent{0.0L};
    auto numerical_slack{0.0L};
    auto numerical_complete{true};

    for (auto const& entry : entries) {
        if (!seen_alternatives.insert(entry.alternative_name).second) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Raw-union distribution contains duplicate alternative '" +
                     entry.alternative_name + "'."});
            continue;
        }
        auto const alternative{std::ranges::find(
            analysis.alternatives, entry.alternative_name, &UnionAlternativeAnalysis::name)};
        if (alternative == analysis.alternatives.end()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Raw-union distribution contains unknown alternative '" +
                                              entry.alternative_name + "'."});
            continue;
        }

        ++result.valid_entry_count;
        auto const weighted_extent{alternative->extent_bytes.has_value()
                                       ? checked_multiply(entry.weight, *alternative->extent_bytes)
                                       : std::nullopt};
        auto const weighted_slack{alternative->slack_bytes.has_value()
                                      ? checked_multiply(entry.weight, *alternative->slack_bytes)
                                      : std::nullopt};
        result.entries.push_back({.alternative_name = entry.alternative_name,
                                  .weight = entry.weight,
                                  .extent_bytes = alternative->extent_bytes,
                                  .slack_bytes = alternative->slack_bytes,
                                  .weighted_extent_bytes = weighted_extent,
                                  .weighted_slack_bytes = weighted_slack});
        numerical_weight += static_cast<long double>(entry.weight);
        if (alternative->extent_bytes.has_value() && alternative->slack_bytes.has_value()) {
            numerical_extent += static_cast<long double>(entry.weight) *
                                static_cast<long double>(*alternative->extent_bytes);
            numerical_slack += static_cast<long double>(entry.weight) *
                               static_cast<long double>(*alternative->slack_bytes);
        } else if (entry.weight != 0) {
            numerical_complete = false;
        }

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_extent_bytes.has_value()) {
            result.total_extent_bytes =
                weighted_extent.has_value()
                    ? checked_add(*result.total_extent_bytes, *weighted_extent)
                    : std::nullopt;
            if (!result.total_extent_bytes.has_value() && !exact_extent_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution extent total overflows or is Unknown."});
                exact_extent_overflow_reported = true;
            }
        }
        if (result.total_slack_bytes.has_value()) {
            result.total_slack_bytes = weighted_slack.has_value()
                                         ? checked_add(*result.total_slack_bytes, *weighted_slack)
                                         : std::nullopt;
            if (!result.total_slack_bytes.has_value() && !exact_slack_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution slack total overflows or is Unknown."});
                exact_slack_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L && numerical_complete) {
        result.expected_extent_bytes_per_value = numerical_extent / numerical_weight;
        result.expected_slack_bytes_per_value = numerical_slack / numerical_weight;
        result.expected_selected_extent_bytes = *result.expected_extent_bytes_per_value *
                                                static_cast<long double>(selected_element_count);
        result.expected_selected_slack_bytes = *result.expected_slack_bytes_per_value *
                                               static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected raw-union alternative usage is Unknown because the distribution has no "
             "positive valid weight with complete target facts."});
    }
    return result;
}

auto Analyzer::compare_union_distributions(UnionDistributionAnalysis const& first,
                                           UnionDistributionAnalysis const& second)
    -> UnionDistributionComparison {
    UnionDistributionComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target raw-union distribution: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target raw-union distribution: " + diagnostic.message});
    }
    if (first.type != second.type ||
        first.selected_element_count != second.selected_element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions use different types or selected counts."});
        return result;
    }
    if (first.valid_entry_count != second.valid_entry_count ||
        first.entries.size() != second.entries.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions do not contain the same valid entries."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    for (auto const& entry : first.entries) {
        first_names.insert(entry.alternative_name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& entry : second.entries) {
        second_names.insert(entry.alternative_name);
    }
    if (first_names.size() != first.entries.size() ||
        second_names.size() != second.entries.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions do not contain the same unique alternatives."});
        return result;
    }
    for (auto const& first_entry : first.entries) {
        auto const second_entry{
            std::ranges::find(second.entries,
                              first_entry.alternative_name,
                              &UnionDistributionEntryAnalysis::alternative_name)};
        if (second_entry == second.entries.end() || first_entry.weight != second_entry->weight) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Compared raw-union distribution entry '" +
                                              first_entry.alternative_name +
                                              "' has an incompatible weight."});
            return result;
        }
    }

    result.entries.reserve(first.entries.size());
    for (auto const& first_entry : first.entries) {
        auto const second_entry{
            std::ranges::find(second.entries,
                              first_entry.alternative_name,
                              &UnionDistributionEntryAnalysis::alternative_name)};
        result.entries.push_back(
            {.alternative_name = first_entry.alternative_name,
             .first = first_entry,
             .second = *second_entry,
             .extent_delta = numeric_delta(first_entry.extent_bytes, second_entry->extent_bytes),
             .slack_delta = numeric_delta(first_entry.slack_bytes, second_entry->slack_bytes),
             .weighted_extent_delta = numeric_delta(first_entry.weighted_extent_bytes,
                                                    second_entry->weighted_extent_bytes),
             .weighted_slack_delta = numeric_delta(first_entry.weighted_slack_bytes,
                                                   second_entry->weighted_slack_bytes)});
    }
    result.total_weight_delta = numeric_delta(first.total_weight, second.total_weight);
    result.total_extent_delta = numeric_delta(first.total_extent_bytes, second.total_extent_bytes);
    result.total_slack_delta = numeric_delta(first.total_slack_bytes, second.total_slack_bytes);
    auto decimal_delta = [](std::optional<long double> const first_value,
                            std::optional<long double> const second_value) {
        return first_value.has_value() && second_value.has_value()
                 ? std::optional{*second_value - *first_value}
                 : std::nullopt;
    };
    result.expected_extent_per_value_delta = decimal_delta(first.expected_extent_bytes_per_value,
                                                           second.expected_extent_bytes_per_value);
    result.expected_slack_per_value_delta =
        decimal_delta(first.expected_slack_bytes_per_value, second.expected_slack_bytes_per_value);
    result.expected_selected_extent_delta =
        decimal_delta(first.expected_selected_extent_bytes, second.expected_selected_extent_bytes);
    result.expected_selected_slack_delta =
        decimal_delta(first.expected_selected_slack_bytes, second.expected_selected_slack_bytes);
    return result;
}

auto Analyzer::analyze_tagged_union_distribution(
    TaggedUnionAnalysis const& tagged_union,
    std::span<TaggedUnionDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> TaggedUnionDistributionAnalysis {
    TaggedUnionDistributionAnalysis result{.type = tagged_union.type,
                                           .valid_entry_count = 0,
                                           .total_weight = 0,
                                           .total_payload_extent_bytes = 0,
                                           .total_payload_slack_bytes = 0,
                                           .expected_payload_extent_bytes_per_value = std::nullopt,
                                           .expected_payload_slack_bytes_per_value = std::nullopt,
                                           .selected_element_count = selected_element_count,
                                           .expected_selected_payload_extent_bytes = std::nullopt,
                                           .expected_selected_payload_slack_bytes = std::nullopt,
                                           .entries = {},
                                           .diagnostics = {}};
    std::set<std::string, std::less<>> seen_tags;
    auto exact_weight_overflow_reported{false};
    auto exact_extent_overflow_reported{false};
    auto exact_slack_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_extent{0.0L};
    auto numerical_slack{0.0L};
    auto numerical_complete{true};

    for (auto const& entry : entries) {
        if (!seen_tags.insert(entry.tag).second) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union distribution contains duplicate tag '" + entry.tag + "'."});
            continue;
        }
        auto const alternative{std::ranges::find(
            tagged_union.alternatives, entry.tag, &TaggedUnionAlternativeAnalysis::tag)};
        if (alternative == tagged_union.alternatives.end()) {
            auto role{std::string{"unknown"}};
            if (std::ranges::find(tagged_union.unmapped_live_tags, entry.tag) !=
                tagged_union.unmapped_live_tags.end()) {
                role = "unmapped live";
            } else if (std::ranges::find(tagged_union.sentinel_tags, entry.tag) !=
                       tagged_union.sentinel_tags.end()) {
                role = "sentinel";
            } else if (tagged_union.count_sentinel_tag == entry.tag) {
                role = "count sentinel";
            }
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Tagged-union distribution tag '" + entry.tag + "' is " +
                                              role + " and has no payload alternative."});
            continue;
        }

        ++result.valid_entry_count;
        auto const weighted_extent{alternative->extent_bytes.has_value()
                                       ? checked_multiply(entry.weight, *alternative->extent_bytes)
                                       : std::nullopt};
        auto const weighted_slack{
            alternative->payload_slack_bytes.has_value()
                ? checked_multiply(entry.weight, *alternative->payload_slack_bytes)
                : std::nullopt};
        result.entries.push_back({.tag = entry.tag,
                                  .alternative_name = alternative->name,
                                  .weight = entry.weight,
                                  .payload_extent_bytes = alternative->extent_bytes,
                                  .payload_slack_bytes = alternative->payload_slack_bytes,
                                  .weighted_payload_extent_bytes = weighted_extent,
                                  .weighted_payload_slack_bytes = weighted_slack});
        numerical_weight += static_cast<long double>(entry.weight);
        if (alternative->extent_bytes.has_value() && alternative->payload_slack_bytes.has_value()) {
            numerical_extent += static_cast<long double>(entry.weight) *
                                static_cast<long double>(*alternative->extent_bytes);
            numerical_slack += static_cast<long double>(entry.weight) *
                               static_cast<long double>(*alternative->payload_slack_bytes);
        } else if (entry.weight != 0) {
            numerical_complete = false;
        }

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_payload_extent_bytes.has_value()) {
            result.total_payload_extent_bytes =
                weighted_extent.has_value()
                    ? checked_add(*result.total_payload_extent_bytes, *weighted_extent)
                    : std::nullopt;
            if (!result.total_payload_extent_bytes.has_value() && !exact_extent_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution payload-extent total overflows or is Unknown."});
                exact_extent_overflow_reported = true;
            }
        }
        if (result.total_payload_slack_bytes.has_value()) {
            result.total_payload_slack_bytes =
                weighted_slack.has_value()
                    ? checked_add(*result.total_payload_slack_bytes, *weighted_slack)
                    : std::nullopt;
            if (!result.total_payload_slack_bytes.has_value() && !exact_slack_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution payload-slack total overflows or is Unknown."});
                exact_slack_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L && numerical_complete) {
        result.expected_payload_extent_bytes_per_value = numerical_extent / numerical_weight;
        result.expected_payload_slack_bytes_per_value = numerical_slack / numerical_weight;
        result.expected_selected_payload_extent_bytes =
            *result.expected_payload_extent_bytes_per_value *
            static_cast<long double>(selected_element_count);
        result.expected_selected_payload_slack_bytes =
            *result.expected_payload_slack_bytes_per_value *
            static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected tagged-union payload usage is Unknown because the distribution has no "
             "positive valid weight with complete target facts."});
    }
    return result;
}

auto Analyzer::compare_tagged_union_distributions(TaggedUnionDistributionAnalysis const& first,
                                                  TaggedUnionDistributionAnalysis const& second)
    -> TaggedUnionDistributionComparison {
    TaggedUnionDistributionComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target tagged-union distribution: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity,
             "Second target tagged-union distribution: " + diagnostic.message});
    }
    if (first.type != second.type ||
        first.selected_element_count != second.selected_element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions use different types or selected counts."});
        return result;
    }
    if (first.valid_entry_count != second.valid_entry_count ||
        first.entries.size() != second.entries.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions do not contain the same valid entries."});
        return result;
    }

    std::set<std::string, std::less<>> first_tags;
    for (auto const& entry : first.entries) {
        first_tags.insert(entry.tag);
    }
    std::set<std::string, std::less<>> second_tags;
    for (auto const& entry : second.entries) {
        second_tags.insert(entry.tag);
    }
    if (first_tags.size() != first.entries.size() || second_tags.size() != second.entries.size() ||
        first_tags != second_tags) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions do not contain the same unique tags."});
        return result;
    }
    for (auto const& first_entry : first.entries) {
        auto const second_entry{std::ranges::find(
            second.entries, first_entry.tag, &TaggedUnionDistributionEntryAnalysis::tag)};
        if (second_entry == second.entries.end() ||
            first_entry.alternative_name != second_entry->alternative_name ||
            first_entry.weight != second_entry->weight) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared tagged-union distribution entry '" + first_entry.tag +
                     "' has incompatible alternative identity or weight."});
            return result;
        }
    }

    result.entries.reserve(first.entries.size());
    for (auto const& first_entry : first.entries) {
        auto const second_entry{std::ranges::find(
            second.entries, first_entry.tag, &TaggedUnionDistributionEntryAnalysis::tag)};
        result.entries.push_back(
            {.tag = first_entry.tag,
             .first = first_entry,
             .second = *second_entry,
             .payload_extent_delta = numeric_delta(first_entry.payload_extent_bytes,
                                                   second_entry->payload_extent_bytes),
             .payload_slack_delta =
                 numeric_delta(first_entry.payload_slack_bytes, second_entry->payload_slack_bytes),
             .weighted_payload_extent_delta =
                 numeric_delta(first_entry.weighted_payload_extent_bytes,
                               second_entry->weighted_payload_extent_bytes),
             .weighted_payload_slack_delta =
                 numeric_delta(first_entry.weighted_payload_slack_bytes,
                               second_entry->weighted_payload_slack_bytes)});
    }
    result.total_weight_delta = numeric_delta(first.total_weight, second.total_weight);
    result.total_payload_extent_delta =
        numeric_delta(first.total_payload_extent_bytes, second.total_payload_extent_bytes);
    result.total_payload_slack_delta =
        numeric_delta(first.total_payload_slack_bytes, second.total_payload_slack_bytes);
    auto decimal_delta = [](std::optional<long double> const first_value,
                            std::optional<long double> const second_value) {
        return first_value.has_value() && second_value.has_value()
                 ? std::optional{*second_value - *first_value}
                 : std::nullopt;
    };
    result.expected_payload_extent_per_value_delta =
        decimal_delta(first.expected_payload_extent_bytes_per_value,
                      second.expected_payload_extent_bytes_per_value);
    result.expected_payload_slack_per_value_delta =
        decimal_delta(first.expected_payload_slack_bytes_per_value,
                      second.expected_payload_slack_bytes_per_value);
    result.expected_selected_payload_extent_delta =
        decimal_delta(first.expected_selected_payload_extent_bytes,
                      second.expected_selected_payload_extent_bytes);
    result.expected_selected_payload_slack_delta = decimal_delta(
        first.expected_selected_payload_slack_bytes, second.expected_selected_payload_slack_bytes);
    return result;
}

auto Analyzer::analyze_record_access(RecordAnalysis const& record,
                                     std::span<AccessIntent const> const accesses,
                                     AbiProfile const& abi,
                                     std::uint64_t const multiplicity) -> RecordAccessAnalysis {
    RecordAccessAnalysis result{.type = record.type,
                                .member_names = {},
                                .accesses = {},
                                .element_count = record.aggregate.element_count,
                                .multiplicity = multiplicity,
                                .useful_bytes = std::nullopt,
                                .read_useful_bytes = std::uint64_t{},
                                .write_useful_bytes = std::uint64_t{},
                                .logical_read_useful_bytes = std::nullopt,
                                .logical_write_useful_bytes = std::nullopt,
                                .object_footprint_bytes = record.aggregate.total_storage_bytes,
                                .cache_line_bytes = std::nullopt,
                                .cache_lines_touched = std::nullopt,
                                .cache_bytes_touched = std::nullopt,
                                .read_cache_lines_touched = std::nullopt,
                                .read_cache_bytes_touched = std::nullopt,
                                .write_cache_lines_touched = std::nullopt,
                                .write_cache_bytes_touched = std::nullopt,
                                .non_selected_cache_bytes = std::nullopt,
                                .cache_footprint_capacity =
                                    cache_capacity_analysis(std::nullopt, abi.memory_facts()),
                                .page_bytes = std::nullopt,
                                .pages_touched = std::nullopt,
                                .page_bytes_touched = std::nullopt,
                                .read_pages_touched = std::nullopt,
                                .read_page_bytes_touched = std::nullopt,
                                .write_pages_touched = std::nullopt,
                                .write_page_bytes_touched = std::nullopt,
                                .non_selected_page_bytes = std::nullopt,
                                .diagnostics = {}};
    if (multiplicity == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Access multiplicity must be non-zero."});
    }
    if (!record.size_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Record has incomplete target layout facts."});
    }

    std::set<std::string, std::less<>> unique_names;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> member_layouts;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> read_member_layouts;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> write_member_layouts;
    std::uint64_t useful_bytes_per_element{};
    std::optional<std::uint64_t> read_bytes_per_element{std::uint64_t{}};
    std::optional<std::uint64_t> write_bytes_per_element{std::uint64_t{}};
    for (auto const& access : accesses) {
        auto const& member_name{access.name};
        if (!unique_names.insert(member_name).second) {
            auto const existing{
                std::ranges::find(result.accesses, member_name, &AccessIntent::name)};
            if (existing != result.accesses.end() && existing->operation != access.operation) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected record member '" + member_name +
                         "' has conflicting access classifications; the first is used."});
            }
            continue;
        }
        auto const member{
            std::ranges::find(record.members, member_name, &RecordMemberAnalysis::name)};
        if (member == record.members.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected record member '" + member_name + "' no longer exists."});
            continue;
        }
        result.member_names.push_back(member_name);
        result.accesses.push_back(access);
        if (!member->offset_bytes.has_value() || !member->extent_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Selected record member '" + member_name +
                                              "' has incomplete target layout facts."});
            if (access.operation != AccessOperation::write) {
                read_bytes_per_element.reset();
            }
            if (access.operation != AccessOperation::read) {
                write_bytes_per_element.reset();
            }
            continue;
        }
        member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        if (access.operation != AccessOperation::write) {
            read_member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        }
        if (access.operation != AccessOperation::read) {
            write_member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        }
        auto const next{checked_add(useful_bytes_per_element, *member->extent_bytes)};
        if (!next.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected member byte count overflows uint64."});
            return result;
        }
        useful_bytes_per_element = *next;

        auto accumulate_classified = [&](std::optional<std::uint64_t>& total,
                                         bool const included,
                                         char const* const category) {
            if (!included || !total.has_value()) {
                return;
            }
            total = checked_add(*total, *member->extent_bytes);
            if (!total.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              std::string{"Selected member "} + category +
                                                  " byte count per element overflows uint64."});
            }
        };
        accumulate_classified(
            read_bytes_per_element, access.operation != AccessOperation::write, "read");
        accumulate_classified(
            write_bytes_per_element, access.operation != AccessOperation::read, "write");
    }
    if (member_layouts.empty()) {
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No record members are selected for access."});
        }
        return result;
    }
    if (!record.size_bytes.has_value()) {
        return result;
    }
    std::ranges::sort(member_layouts);
    std::ranges::sort(read_member_layouts);
    std::ranges::sort(write_member_layouts);
    result.useful_bytes = checked_multiply(useful_bytes_per_element, result.element_count);
    if (!result.useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected member useful byte count overflows uint64."});
    }
    result.read_useful_bytes = read_bytes_per_element.and_then(
        [&](std::uint64_t const bytes) { return checked_multiply(bytes, result.element_count); });
    result.write_useful_bytes = write_bytes_per_element.and_then(
        [&](std::uint64_t const bytes) { return checked_multiply(bytes, result.element_count); });
    if (read_bytes_per_element.has_value() && !result.read_useful_bytes.has_value()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Selected member read useful byte count overflows uint64."});
    }
    if (write_bytes_per_element.has_value() && !result.write_useful_bytes.has_value()) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "Selected member write useful byte count overflows uint64."});
    }

    auto analyze_regions = [&](std::optional<std::uint64_t> const region_bytes,
                               char const* const region_name,
                               std::optional<std::uint64_t>& output_regions,
                               std::optional<std::uint64_t>& output_region_bytes) {
        output_region_bytes = region_bytes;
        if (!region_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          std::string{region_name} +
                                              " size is unknown for ABI profile '" + abi.name() +
                                              "'."});
            return;
        }
        if (*region_bytes == 0) {
            output_region_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + " size must be non-zero."});
            return;
        }
        std::string error;
        output_regions = touched_regions(
            *record.size_bytes, member_layouts, result.element_count, *region_bytes, error);
        if (!output_regions.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + ": " + error});
        }
    };
    auto const& memory{abi.memory_facts()};
    analyze_regions(
        memory.cache_line_bytes, "Cache-line", result.cache_lines_touched, result.cache_line_bytes);
    analyze_regions(memory.page_bytes, "Page", result.pages_touched, result.page_bytes);

    auto analyze_operation_regions = [&](auto const& layouts,
                                         std::optional<std::uint64_t> const region_bytes,
                                         std::optional<std::uint64_t>& output_regions,
                                         std::optional<std::uint64_t>& output_bytes,
                                         char const* const category) {
        if (layouts.empty()) {
            output_regions = 0;
            output_bytes = 0;
            return;
        }
        if (!region_bytes.has_value() || *region_bytes == 0) {
            return;
        }
        std::string error;
        output_regions = touched_regions(
            *record.size_bytes, layouts, result.element_count, *region_bytes, error);
        if (!output_regions.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{category} + ": " + error});
            return;
        }
        output_bytes = checked_multiply(*output_regions, *region_bytes);
        if (!output_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{category} + " address-coverage byte count overflows uint64."});
        }
    };
    analyze_operation_regions(read_member_layouts,
                              memory.cache_line_bytes,
                              result.read_cache_lines_touched,
                              result.read_cache_bytes_touched,
                              "Read cache-line");
    analyze_operation_regions(write_member_layouts,
                              memory.cache_line_bytes,
                              result.write_cache_lines_touched,
                              result.write_cache_bytes_touched,
                              "Write cache-line");
    analyze_operation_regions(read_member_layouts,
                              memory.page_bytes,
                              result.read_pages_touched,
                              result.read_page_bytes_touched,
                              "Read page");
    analyze_operation_regions(write_member_layouts,
                              memory.page_bytes,
                              result.write_pages_touched,
                              result.write_page_bytes_touched,
                              "Write page");

    auto derive_footprint = [&](std::optional<std::uint64_t> const region_count,
                                std::optional<std::uint64_t> const region_bytes,
                                std::optional<std::uint64_t>& footprint_bytes,
                                std::optional<std::uint64_t>& non_selected_bytes,
                                char const* const region_name) {
        if (!region_count.has_value() || !region_bytes.has_value()) {
            return;
        }
        footprint_bytes = checked_multiply(*region_count, *region_bytes);
        if (!footprint_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{"Touched "} + region_name + " byte count overflows uint64."});
            return;
        }
        if (!result.useful_bytes.has_value()) {
            return;
        }
        if (*footprint_bytes < *result.useful_bytes) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          std::string{"Touched "} + region_name +
                                              " bytes are smaller than useful member bytes."});
            return;
        }
        non_selected_bytes = *footprint_bytes - *result.useful_bytes;
    };
    derive_footprint(result.cache_lines_touched,
                     result.cache_line_bytes,
                     result.cache_bytes_touched,
                     result.non_selected_cache_bytes,
                     "cache-line");
    derive_footprint(result.pages_touched,
                     result.page_bytes,
                     result.page_bytes_touched,
                     result.non_selected_page_bytes,
                     "page");
    result.cache_footprint_capacity =
        cache_capacity_analysis(result.cache_bytes_touched, abi.memory_facts());
    result.logical_read_useful_bytes = logical_useful_bytes(result.read_useful_bytes, multiplicity);
    result.logical_write_useful_bytes =
        logical_useful_bytes(result.write_useful_bytes, multiplicity);
    if (multiplicity != 0 && result.read_useful_bytes.has_value() &&
        !result.logical_read_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical read useful byte total overflows uint64."});
    }
    if (multiplicity != 0 && result.write_useful_bytes.has_value() &&
        !result.logical_write_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical write useful byte total overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_record_access(RecordAnalysis const& record,
                                     std::span<std::string const> const member_names,
                                     AbiProfile const& abi,
                                     AccessOperation const operation,
                                     std::uint64_t const multiplicity) -> RecordAccessAnalysis {
    std::vector<AccessIntent> accesses;
    accesses.reserve(member_names.size());
    for (auto const& member_name : member_names) {
        accesses.push_back({.name = member_name, .operation = operation});
    }
    return analyze_record_access(record, accesses, abi, multiplicity);
}

auto Analyzer::analyze_record_member_access(RecordAnalysis const& record,
                                            std::string_view const member_name,
                                            AbiProfile const& abi,
                                            AccessOperation const operation,
                                            std::uint64_t const multiplicity)
    -> RecordAccessAnalysis {
    auto const names{std::vector<std::string>{std::string{member_name}}};
    return analyze_record_access(record, names, abi, operation, multiplicity);
}

auto Analyzer::compare_record_access(RecordAccessAnalysis const& first,
                                     RecordAccessAnalysis const& second) -> RecordAccessComparison {
    RecordAccessComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target record access: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target record access: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses describe different records."});
        return result;
    }
    if (first.element_count != second.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different element counts."});
        return result;
    }
    if (first.multiplicity != second.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different access multiplicities."});
        return result;
    }

    auto unique_names{[](auto const& values, auto const projection) {
        std::set<std::string, std::less<>> names;
        for (auto const& value : values) {
            names.insert(projection(value));
        }
        return names;
    }};
    auto const first_member_names{unique_names(
        first.member_names, [](std::string const& name) -> std::string const& { return name; })};
    auto const second_member_names{unique_names(
        second.member_names, [](std::string const& name) -> std::string const& { return name; })};
    auto const first_access_names{
        unique_names(first.accesses,
                     [](AccessIntent const& access) -> std::string const& { return access.name; })};
    auto const second_access_names{
        unique_names(second.accesses,
                     [](AccessIntent const& access) -> std::string const& { return access.name; })};
    if (first_member_names.size() != first.member_names.size() ||
        second_member_names.size() != second.member_names.size() ||
        first_access_names.size() != first.accesses.size() ||
        second_access_names.size() != second.accesses.size() ||
        first_member_names != first_access_names || second_member_names != second_access_names ||
        first_member_names != second_member_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses do not contain the same complete unique selected "
             "member set."});
        return result;
    }
    if (!access_intents_match(first.accesses, second.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared record access analyses use different per-member access classifications."});
        return result;
    }

    result.useful_byte_delta = numeric_delta(first.useful_bytes, second.useful_bytes);
    result.read_useful_byte_delta =
        numeric_delta(first.read_useful_bytes, second.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(first.write_useful_bytes, second.write_useful_bytes);
    result.logical_read_useful_byte_delta =
        numeric_delta(first.logical_read_useful_bytes, second.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta =
        numeric_delta(first.logical_write_useful_bytes, second.logical_write_useful_bytes);
    result.object_footprint_byte_delta =
        numeric_delta(first.object_footprint_bytes, second.object_footprint_bytes);
    result.cache_line_size_delta = numeric_delta(first.cache_line_bytes, second.cache_line_bytes);
    result.cache_line_delta = numeric_delta(first.cache_lines_touched, second.cache_lines_touched);
    result.cache_byte_delta = numeric_delta(first.cache_bytes_touched, second.cache_bytes_touched);
    result.read_cache_line_delta =
        numeric_delta(first.read_cache_lines_touched, second.read_cache_lines_touched);
    result.read_cache_byte_delta =
        numeric_delta(first.read_cache_bytes_touched, second.read_cache_bytes_touched);
    result.write_cache_line_delta =
        numeric_delta(first.write_cache_lines_touched, second.write_cache_lines_touched);
    result.write_cache_byte_delta =
        numeric_delta(first.write_cache_bytes_touched, second.write_cache_bytes_touched);
    result.non_selected_cache_byte_delta =
        numeric_delta(first.non_selected_cache_bytes, second.non_selected_cache_bytes);
    result.page_size_delta = numeric_delta(first.page_bytes, second.page_bytes);
    result.page_delta = numeric_delta(first.pages_touched, second.pages_touched);
    result.page_byte_delta = numeric_delta(first.page_bytes_touched, second.page_bytes_touched);
    result.read_page_delta = numeric_delta(first.read_pages_touched, second.read_pages_touched);
    result.read_page_byte_delta =
        numeric_delta(first.read_page_bytes_touched, second.read_page_bytes_touched);
    result.write_page_delta = numeric_delta(first.write_pages_touched, second.write_pages_touched);
    result.write_page_byte_delta =
        numeric_delta(first.write_page_bytes_touched, second.write_page_bytes_touched);
    result.non_selected_page_byte_delta =
        numeric_delta(first.non_selected_page_bytes, second.non_selected_page_bytes);
    return result;
}

auto Analyzer::analyze_packed(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              Variant const& variant,
                              AbiProfile const& abi,
                              std::uint64_t const element_count,
                              std::span<RelationshipTargetFacts const> const relationship_targets)
    -> PackedAnalysis {
    auto const& packed{std::get<lispb::schema::PackedType>(types.type(type).definition)};
    auto const schema_storage{physical_type_spelling(types, packed.storage_type.type)
                                  .value_or(packed.storage_type.cpp_type.spelling)};
    PackedAnalysis result{.type = type,
                          .schema_storage_type = schema_storage,
                          .storage_type = effective_storage_type(types, type, packed, variant),
                          .storage_overridden =
                              variant.overrides.packed_storage_types.contains(type),
                          .byte_order = packed.byte_order,
                          .bit_order = packed.bit_order,
                          .storage_facts = std::nullopt,
                          .storage_bits = std::nullopt,
                          .bits_used = std::nullopt,
                          .payload_bits = std::nullopt,
                          .reserved_bits = std::nullopt,
                          .unused_bits = std::nullopt,
                          .overflow_bits = std::nullopt,
                          .invalid_raw_value = packed.invalid_raw_value,
                          .fields = {},
                          .diagnostics = {},
                          .aggregate = {.element_count = element_count,
                                        .total_storage_bytes = std::nullopt,
                                        .total_payload_bits = std::nullopt,
                                        .total_reserved_bits = std::nullopt,
                                        .total_unused_bits = std::nullopt,
                                        .cache_line_bytes = std::nullopt,
                                        .minimum_cache_lines = std::nullopt,
                                        .complete_elements_per_cache_line = std::nullopt,
                                        .cache_line_straddling_elements = std::nullopt,
                                        .page_bytes = std::nullopt,
                                        .minimum_pages = std::nullopt,
                                        .complete_elements_per_page = std::nullopt,
                                        .page_straddling_elements = std::nullopt,
                                        .cache_capacity = {}}};
    result.storage_facts = abi.find(result.storage_type);
    if (!result.storage_facts.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
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
    result.fields.reserve(packed.segments.size());
    for (auto const& segment : packed.segments) {
        auto const* field{std::get_if<lispb::schema::PackedField>(&segment)};
        auto const* reserved{std::get_if<lispb::schema::PackedReservedBits>(&segment)};
        auto const field_override{field != nullptr &&
                                  variant.overrides.packed_field_widths.contains(
                                      FieldOverrideId{.type = type, .field_name = field->name})};
        auto const representation_owned_width{
            field != nullptr && (field->kind == codegen::PackedFieldKind::linear_quantized ||
                                 field->kind == codegen::PackedFieldKind::fixed_point)};
        auto const width{field != nullptr && !representation_owned_width
                             ? effective_field_width(type, *field, variant)
                         : field != nullptr ? field->bit_width
                                            : reserved->bit_width};
        auto const next_offset{checked_add(offset, width)};
        auto least_significant_bit{offset};
        if (packed.bit_order == codegen::PackedBitOrder::most_significant_first) {
            least_significant_bit = 0;
            if (result.storage_bits.has_value() && next_offset.has_value() &&
                *next_offset <= *result.storage_bits) {
                least_significant_bit = *result.storage_bits - *next_offset;
            }
        }
        PackedFieldAnalysis field_result{
            .name = field != nullptr ? field->name : reserved->name,
            .semantic_type = field != nullptr
                               ? std::optional<lispb::schema::TypeId>{field->semantic_type.type}
                               : std::nullopt,
            .logical_type = field != nullptr ? types.type(field->semantic_type.type).cpp_spelling
                                             : "Reserved bits",
            .kind = field != nullptr ? field->kind : codegen::PackedFieldKind::unsigned_integer,
            .reserved = reserved != nullptr,
            .schema_bit_width = field != nullptr ? field->bit_width : reserved->bit_width,
            .schema_bit_width_auto = field != nullptr && field->bit_width_auto,
            .bit_width = width,
            .overridden = field_override && !representation_owned_width,
            .least_significant_bit = least_significant_bit,
            .most_significant_bit = std::nullopt,
            .maximum_unsigned_value =
                field != nullptr ? maximum_unsigned_value(width) : std::nullopt,
            .minimum_signed_value =
                field != nullptr && field->kind == codegen::PackedFieldKind::signed_integer
                    ? minimum_signed_value(width)
                    : std::nullopt,
            .maximum_signed_value =
                field != nullptr && field->kind == codegen::PackedFieldKind::signed_integer
                    ? maximum_signed_value(width)
                    : std::nullopt,
            .minimum_semantic_value = field != nullptr ? field->minimum_value : std::nullopt,
            .maximum_semantic_value = field != nullptr ? field->maximum_value : std::nullopt,
            .semantic_value_count = std::nullopt,
            .sentinel_code_count = 0,
            .required_code_count = std::nullopt,
            .minimum_required_bits = std::nullopt,
            .unused_codes = std::nullopt,
            .named_codes = {},
            .linear_quantized = std::nullopt,
            .fixed_point = std::nullopt,
            .relationship_kind = field != nullptr && field->relationship.has_value()
                                   ? std::optional{field->relationship->kind}
                                   : std::nullopt,
            .relationship_unit = field != nullptr && field->relationship.has_value()
                                   ? field->relationship->unit
                                   : std::nullopt,
            .relationship_target =
                field != nullptr && field->relationship.has_value()
                    ? std::optional{types.type(field->relationship->target.type).identity.name}
                    : std::nullopt,
            .relationship_target_extent = std::nullopt,
            .relationship_live_value_count = std::nullopt,
            .relationship_required_code_count = std::nullopt,
            .relationship_minimum_required_bits = std::nullopt,
            .relationship_width_sufficient = std::nullopt,
            .relationship_code_space_capacity_limit = std::nullopt,
            .relationship_capacity_headroom = std::nullopt,
            .relationship_semantic_capacity_limit = std::nullopt,
            .relationship_sentinel_capacity_limit = std::nullopt,
            .relationship_effective_capacity_limit = std::nullopt,
            .relationship_effective_capacity_headroom = std::nullopt};
        if (width == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed segment '" + field_result.name + "' must use at least one bit."});
        }
        if (field_override && representation_owned_width) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Packed field '" + field->name +
                     "' ignores a session width override because its " +
                     std::string{field->kind == codegen::PackedFieldKind::linear_quantized
                                     ? "linear-quantized"
                                     : "fixed-point"} +
                     " representation owns the exact encoded width."});
        }
        if (field != nullptr && !field_result.maximum_unsigned_value.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field->name + "' exceeds the 64-bit V1 range limit."});
        }
        std::optional<codegen::PackedIntegerValue> minimum_required_code;
        std::optional<codegen::PackedIntegerValue> maximum_required_code;
        if (field != nullptr) {
            if (std::holds_alternative<lispb::schema::LinearQuantizedType>(
                    types.type(field->semantic_type.type).definition)) {
                field_result.linear_quantized =
                    analyze_linear_quantized(types, field->semantic_type.type);
            } else if (std::holds_alternative<lispb::schema::FixedPointType>(
                           types.type(field->semantic_type.type).definition)) {
                field_result.fixed_point = analyze_fixed_point(types, field->semantic_type.type);
            }
            field_result.named_codes.reserve(field->named_codes.size());
            for (auto const& code : field->named_codes) {
                field_result.named_codes.push_back(
                    {.name = code.name, .value = code.value, .sentinel = code.sentinel});
                if (code.sentinel) {
                    ++field_result.sentinel_code_count;
                }
                if (!minimum_required_code.has_value() ||
                    codegen::packed_integer_less(code.value, *minimum_required_code)) {
                    minimum_required_code = code.value;
                }
                if (!maximum_required_code.has_value() ||
                    codegen::packed_integer_less(*maximum_required_code, code.value)) {
                    maximum_required_code = code.value;
                }
            }
        }
        if (field_result.minimum_semantic_value.has_value() &&
            field_result.maximum_semantic_value.has_value()) {
            auto const minimum{*field_result.minimum_semantic_value};
            auto const maximum{*field_result.maximum_semantic_value};
            field_result.semantic_value_count = packed_range_count(minimum, maximum);
            if (!minimum_required_code.has_value() ||
                codegen::packed_integer_less(minimum, *minimum_required_code)) {
                minimum_required_code = minimum;
            }
            if (!maximum_required_code.has_value() ||
                codegen::packed_integer_less(*maximum_required_code, maximum)) {
                maximum_required_code = maximum;
            }
            if (field_result.semantic_value_count.has_value()) {
                field_result.required_code_count = checked_add(*field_result.semantic_value_count,
                                                               field_result.sentinel_code_count);
            }
            auto const signed_domain{field->kind == codegen::PackedFieldKind::signed_integer};
            auto const domain_fits{
                minimum_required_code.has_value() && maximum_required_code.has_value() &&
                (signed_domain
                     ? codegen::packed_integer_fits_signed(*minimum_required_code, width) &&
                           codegen::packed_integer_fits_signed(*maximum_required_code, width)
                     : codegen::packed_integer_fits_unsigned(*minimum_required_code, width) &&
                           codegen::packed_integer_fits_unsigned(*maximum_required_code, width))};
            if (!domain_fits) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed field '" + field->name +
                         "' semantic range or sentinel codes do not fit its planned width."});
            } else if (width == 64 && field_result.required_code_count.has_value()) {
                field_result.unused_codes = std::numeric_limits<std::uint64_t>::max() -
                                          *field_result.required_code_count + 1;
            } else if (width == 64 && field_result.sentinel_code_count == 0) {
                auto const full_signed_domain{
                    signed_domain && minimum.negative &&
                    minimum.magnitude == (std::uint64_t{1} << 63) && !maximum.negative &&
                    maximum.magnitude ==
                        static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())};
                auto const full_unsigned_domain{!signed_domain && !minimum.negative &&
                                                minimum.magnitude == 0 && !maximum.negative &&
                                                maximum.magnitude ==
                                                    (std::numeric_limits<std::uint64_t>::max)()};
                if (full_signed_domain || full_unsigned_domain) {
                    field_result.unused_codes = 0;
                }
            } else if (field_result.required_code_count.has_value()) {
                field_result.unused_codes =
                    (std::uint64_t{1} << width) - *field_result.required_code_count;
            }
        }
        if (minimum_required_code.has_value() && maximum_required_code.has_value()) {
            field_result.minimum_required_bits = codegen::minimum_packed_integer_bits(
                *minimum_required_code,
                *maximum_required_code,
                field != nullptr && field->kind == codegen::PackedFieldKind::signed_integer);
        }
        if (field != nullptr && field->relationship.has_value() &&
            (field->relationship->kind == codegen::SemanticRelationKind::index_into ||
             field->relationship->kind == codegen::SemanticRelationKind::count_of ||
             field->relationship->kind == codegen::SemanticRelationKind::offset_into)) {
            auto const target_facts{std::ranges::find(relationship_targets,
                                                      field->relationship->target.type,
                                                      &RelationshipTargetFacts::target)};
            if (target_facts != relationship_targets.end()) {
                auto const target_extent{
                    relationship_target_extent(*field->relationship, *target_facts)};
                if (target_extent.has_value()) {
                    auto const extent_analysis{
                        analyze_unsigned_relationship_extent("Packed field '" + field->name + "'",
                                                             field->relationship->kind,
                                                             *target_extent,
                                                             field->named_codes,
                                                             width,
                                                             field->minimum_value,
                                                             field->maximum_value)};
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
                packed.bit_order == codegen::PackedBitOrder::least_significant_first ||
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
            checked_add(field != nullptr ? payload_bits : reserved_bits, width)};
        if (!next_category_bits.has_value()) {
            width_overflow = true;
        } else if (field != nullptr) {
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

    if (packed.invalid_raw_value.has_value() && result.storage_bits.has_value() &&
        *result.storage_bits < 64) {
        auto const storage_max{
            maximum_unsigned_value(static_cast<std::uint32_t>(*result.storage_bits))};
        if (storage_max.has_value() && *packed.invalid_raw_value > *storage_max) {
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
    std::set<std::string, std::less<>> names;
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

    std::set<std::string, std::less<>> unique_names;
    std::uint64_t useful_bits_per_element{};
    std::uint64_t read_bits_per_element{};
    std::uint64_t write_bits_per_element{};
    bool read_selected{};
    bool write_selected{};
    for (auto const& access : accesses) {
        if (!unique_names.insert(access.name).second) {
            auto const existing{
                std::ranges::find(result.accesses, access.name, &AccessIntent::name)};
            if (existing != result.accesses.end() && existing->operation != access.operation) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected packed field '" + access.name +
                         "' has conflicting access classifications; the first is used."});
            }
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
        if (access.operation != AccessOperation::write) {
            read_selected = true;
            if (!accumulate_bits(read_bits_per_element, "read")) {
                return result;
            }
        }
        if (access.operation != AccessOperation::read) {
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
        field_access.read_useful_bits = access.operation != AccessOperation::write
                                          ? field_access.useful_bits
                                          : std::optional<std::uint64_t>{0};
        field_access.write_useful_bits = access.operation != AccessOperation::read
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
    auto const first_names{
        std::set<std::string, std::less<>>{first.field_names.begin(), first.field_names.end()}};
    auto const second_names{
        std::set<std::string, std::less<>>{second.field_names.begin(), second.field_names.end()}};
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

auto Analyzer::analyze_soa(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           Variant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const default_capacity,
                           SoaAllocationStrategy const allocation_strategy) -> SoaAnalysis {
    auto const& soa{std::get<lispb::schema::SoaType>(types.type(type).definition)};
    auto capacity{default_capacity};
    bool capacity_overridden{};
    if (auto const found{variant.overrides.capacities.find(type)};
        found != variant.overrides.capacities.end()) {
        capacity = found->second;
        capacity_overridden = true;
    }
    SoaAnalysis result{.type = type,
                       .capacity = capacity,
                       .capacity_overridden = capacity_overridden,
                       .allocation_strategy = allocation_strategy,
                       .allocation_count =
                           capacity == 0 || soa.columns.empty()
                               ? 0U
                               : (allocation_strategy == SoaAllocationStrategy::separate_columns
                                      ? static_cast<std::uint64_t>(soa.columns.size())
                                      : 1U),
                       .columns = {},
                       .bytes_per_logical_element = std::nullopt,
                       .total_payload_bytes = std::nullopt,
                       .total_allocation_bytes = std::nullopt,
                       .total_alignment_padding_bytes = std::nullopt,
                       .allocation_alignment_bytes = std::nullopt,
                       .cache_line_bytes = abi.memory_facts().cache_line_bytes,
                       .page_bytes = abi.memory_facts().page_bytes,
                       .minimum_pages = std::nullopt,
                       .cache_capacity = {},
                       .diagnostics = {}};
    auto const cache_line_bytes{result.cache_line_bytes};
    auto const has_cache_line_size{cache_line_bytes.has_value() && *cache_line_bytes != 0};
    if (!cache_line_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*cache_line_bytes == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
    }
    auto const has_page_size{result.page_bytes.has_value() && *result.page_bytes != 0};
    if (!result.page_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Page size is unknown for ABI profile '" + abi.name() + "'."});
    } else if (*result.page_bytes == 0) {
        result.page_bytes.reset();
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
    }
    std::uint64_t row_bytes{};
    std::uint64_t total_bytes{};
    std::uint64_t total_pages{};
    bool complete{true};
    bool pages_complete{has_page_size};
    bool allocation_complete{true};
    std::uint64_t allocation_offset{};
    std::uint64_t allocation_padding{};
    std::uint64_t allocation_alignment{1};
    result.columns.reserve(soa.columns.size());

    for (auto const& column : soa.columns) {
        SoaColumnAnalysis column_result{
            .name = column.name,
            .semantic_type = column.semantic_type.type,
            .schema_type = types.type(column.semantic_type.type).cpp_spelling,
            .physical_type = effective_column_type(types, type, column, variant),
            .overridden = variant.overrides.soa_column_types.contains(
                FieldOverrideId{.type = type, .field_name = column.name}),
            .type_facts = std::nullopt,
            .total_bytes = std::nullopt,
            .allocation_offset_bytes = std::nullopt,
            .padding_before_bytes = std::nullopt,
            .minimum_cache_lines = std::nullopt,
            .elements_per_cache_line = std::nullopt,
            .minimum_pages = std::nullopt,
            .complete_elements_per_page = std::nullopt,
            .cache_line_tiling = std::nullopt};
        column_result.type_facts = abi.find(column_result.physical_type);
        if (!column_result.type_facts.has_value()) {
            complete = false;
            pages_complete = false;
            allocation_complete = false;
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Unknown physical facts for SoA column '" + column.name +
                                              "' type '" + column_result.physical_type + "'."});
            result.columns.push_back(std::move(column_result));
            continue;
        }

        auto const size{column_result.type_facts->size_bytes};
        auto const alignment{column_result.type_facts->alignment_bytes};
        if (size == 0 || alignment == 0) {
            complete = false;
            pages_complete = false;
            allocation_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "SoA column '" + column.name + "' has a zero-byte size or zero-byte alignment."});
            result.columns.push_back(std::move(column_result));
            continue;
        }
        column_result.total_bytes = checked_multiply(size, capacity);
        if (!column_result.total_bytes.has_value()) {
            complete = false;
            pages_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Byte count overflows uint64 for SoA column '" + column.name + "'."});
        } else if (has_cache_line_size) {
            column_result.minimum_cache_lines =
                minimum_regions(*column_result.total_bytes, *cache_line_bytes);
        }
        if (has_cache_line_size) {
            column_result.cache_line_tiling = cache_line_tiling(size, *cache_line_bytes);
            column_result.elements_per_cache_line =
                column_result.cache_line_tiling->exact_elements_per_cache_line;
        }
        if (has_page_size) {
            column_result.complete_elements_per_page = *result.page_bytes / size;
            if (column_result.total_bytes.has_value()) {
                column_result.minimum_pages =
                    minimum_regions(*column_result.total_bytes, *result.page_bytes);
                auto const next_pages{checked_add(total_pages, *column_result.minimum_pages)};
                if (!next_pages.has_value()) {
                    pages_complete = false;
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         "SoA minimum page count across separate columns overflows uint64."});
                } else {
                    total_pages = *next_pages;
                }
            }
        }

        auto const next_row_bytes{checked_add(row_bytes, size)};
        if (!next_row_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "SoA bytes per logical element overflow uint64."});
        } else {
            row_bytes = *next_row_bytes;
        }
        if (column_result.total_bytes.has_value()) {
            auto const next_total{checked_add(total_bytes, *column_result.total_bytes)};
            if (!next_total.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "SoA total payload bytes overflow uint64."});
            } else {
                total_bytes = *next_total;
            }
        }
        if (allocation_strategy == SoaAllocationStrategy::aligned_contiguous) {
            auto const aligned{align_up(allocation_offset, alignment)};
            if (!aligned.has_value() || !column_result.total_bytes.has_value()) {
                allocation_complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Aligned contiguous allocation arithmetic overflows at SoA column '" +
                         column.name + "'."});
            } else {
                column_result.allocation_offset_bytes = *aligned;
                column_result.padding_before_bytes = *aligned - allocation_offset;
                auto const next_padding{
                    checked_add(allocation_padding, *column_result.padding_before_bytes)};
                auto const next_offset{checked_add(*aligned, *column_result.total_bytes)};
                if (!next_padding.has_value() || !next_offset.has_value()) {
                    allocation_complete = false;
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         "Aligned contiguous allocation total overflows at SoA column '" +
                             column.name + "'."});
                } else {
                    allocation_padding = *next_padding;
                    allocation_offset = *next_offset;
                    allocation_alignment = std::max(allocation_alignment, alignment);
                }
            }
        }
        result.columns.push_back(std::move(column_result));
    }

    if (complete) {
        result.bytes_per_logical_element = row_bytes;
        result.total_payload_bytes = total_bytes;
    }
    if (allocation_strategy == SoaAllocationStrategy::separate_columns) {
        result.total_allocation_bytes = result.total_payload_bytes;
        result.total_alignment_padding_bytes = 0;
    } else if (allocation_complete) {
        result.total_allocation_bytes = allocation_offset;
        result.total_alignment_padding_bytes = allocation_padding;
        result.allocation_alignment_bytes = allocation_alignment;
    }
    result.cache_capacity =
        cache_capacity_analysis(result.total_allocation_bytes, abi.memory_facts());
    if (allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
        result.total_allocation_bytes.has_value() && has_page_size) {
        result.minimum_pages = minimum_regions(*result.total_allocation_bytes, *result.page_bytes);
    } else if (pages_complete) {
        result.minimum_pages = total_pages;
    }
    return result;
}

auto Analyzer::compare_soa_targets(SoaAnalysis const& first, SoaAnalysis const& second)
    -> SoaTargetComparison {
    SoaTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target SoA layout: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target SoA layout: " + diagnostic.message});
    }

    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses describe different semantic types."});
        return result;
    }
    if (first.capacity != second.capacity ||
        first.capacity_overridden != second.capacity_overridden) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses use different capacities or capacity overrides."});
        return result;
    }
    if (first.allocation_strategy != second.allocation_strategy) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses use different allocation strategies."});
        return result;
    }
    if (first.columns.size() != second.columns.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target SoA analyses do not contain the same ordered columns."});
        return result;
    }

    std::set<std::string, std::less<>> names;
    for (std::size_t index{}; index < first.columns.size(); ++index) {
        auto const& first_column{first.columns[index]};
        auto const& second_column{second.columns[index]};
        auto const unique_name{names.insert(first_column.name).second};
        if (!unique_name || first_column.name != second_column.name ||
            first_column.semantic_type != second_column.semantic_type ||
            first_column.schema_type != second_column.schema_type ||
            first_column.physical_type != second_column.physical_type ||
            first_column.overridden != second_column.overridden) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target SoA analyses do not contain the same unique ordered columns "
                 "and physical variant."});
            return result;
        }
    }

    result.compatible = true;
    result.columns.reserve(first.columns.size());
    for (std::size_t index{}; index < first.columns.size(); ++index) {
        auto const& first_column{first.columns[index]};
        auto const& second_column{second.columns[index]};
        auto const first_size{first_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_column.type_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.columns.push_back(
            {.name = first_column.name,
             .first = first_column,
             .second = second_column,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .total_byte_delta = numeric_delta(first_column.total_bytes, second_column.total_bytes),
             .allocation_offset_delta = numeric_delta(first_column.allocation_offset_bytes,
                                                      second_column.allocation_offset_bytes),
             .padding_before_delta = numeric_delta(first_column.padding_before_bytes,
                                                   second_column.padding_before_bytes),
             .minimum_cache_line_delta =
                 numeric_delta(first_column.minimum_cache_lines, second_column.minimum_cache_lines),
             .elements_per_cache_line_delta = numeric_delta(first_column.elements_per_cache_line,
                                                            second_column.elements_per_cache_line),
             .minimum_page_delta =
                 numeric_delta(first_column.minimum_pages, second_column.minimum_pages),
             .complete_elements_per_page_delta =
                 numeric_delta(first_column.complete_elements_per_page,
                               second_column.complete_elements_per_page)});
    }

    result.allocation_count_delta = numeric_delta(first.allocation_count, second.allocation_count);
    result.bytes_per_logical_element_delta =
        numeric_delta(first.bytes_per_logical_element, second.bytes_per_logical_element);
    result.total_payload_delta =
        numeric_delta(first.total_payload_bytes, second.total_payload_bytes);
    result.total_allocation_delta =
        numeric_delta(first.total_allocation_bytes, second.total_allocation_bytes);
    result.total_alignment_padding_delta =
        numeric_delta(first.total_alignment_padding_bytes, second.total_alignment_padding_bytes);
    result.allocation_alignment_delta =
        numeric_delta(first.allocation_alignment_bytes, second.allocation_alignment_bytes);
    result.cache_line_size_delta = numeric_delta(first.cache_line_bytes, second.cache_line_bytes);
    result.page_size_delta = numeric_delta(first.page_bytes, second.page_bytes);
    result.minimum_page_delta = numeric_delta(first.minimum_pages, second.minimum_pages);
    return result;
}

auto Analyzer::derive_relationship_target_facts(lispb::schema::TypeGraph const& types,
                                                Variant const& variant,
                                                AbiProfile const& abi,
                                                std::uint64_t const default_capacity,
                                                SoaAllocationStrategy const allocation_strategy)
    -> std::vector<RelationshipTargetFacts> {
    std::vector<RelationshipTargetFacts> result;
    auto const resolved_types{types.types()};
    result.reserve(resolved_types.size());

    for (std::size_t type_index{}; type_index < resolved_types.size(); ++type_index) {
        if (!std::holds_alternative<lispb::schema::SoaType>(
                resolved_types[type_index].definition)) {
            continue;
        }

        auto const type{lispb::schema::TypeId{.value = static_cast<std::uint32_t>(type_index)}};
        auto const analysis{
            analyze_soa(types, type, variant, abi, default_capacity, allocation_strategy)};
        result.push_back({.target = type,
                          .element_capacity = analysis.capacity,
                          .byte_extent = analysis.total_allocation_bytes});
    }
    return result;
}

auto Analyzer::analyze_soa_access(SoaAnalysis const& soa,
                                  std::span<AccessIntent const> const accesses,
                                  AbiProfile const& abi,
                                  std::uint64_t const element_count,
                                  std::uint64_t const multiplicity) -> SoaAccessAnalysis {
    SoaAccessAnalysis result{.column_names = {},
                             .accesses = {},
                             .columns = {},
                             .element_count = element_count,
                             .multiplicity = multiplicity,
                             .allocation_strategy = soa.allocation_strategy,
                             .footprint_exact = soa.allocation_strategy ==
                                                SoaAllocationStrategy::aligned_contiguous,
                             .allocation_count = soa.allocation_count,
                             .useful_bytes = std::uint64_t{},
                             .read_useful_bytes = std::uint64_t{},
                             .write_useful_bytes = std::uint64_t{},
                             .logical_read_useful_bytes = std::nullopt,
                             .logical_write_useful_bytes = std::nullopt,
                             .full_logical_payload_bytes = std::nullopt,
                             .unselected_payload_bytes = std::nullopt,
                             .allocated_capacity_payload_bytes = soa.total_payload_bytes,
                             .capacity_slack_payload_bytes = std::nullopt,
                             .total_allocation_bytes = soa.total_allocation_bytes,
                             .alignment_padding_bytes = soa.total_alignment_padding_bytes,
                             .cache_line_bytes = abi.memory_facts().cache_line_bytes,
                             .minimum_cache_lines_touched = std::uint64_t{},
                             .minimum_cache_bytes_touched = std::nullopt,
                             .minimum_read_cache_lines_touched = std::uint64_t{},
                             .minimum_read_cache_bytes_touched = std::nullopt,
                             .minimum_write_cache_lines_touched = std::uint64_t{},
                             .minimum_write_cache_bytes_touched = std::nullopt,
                             .non_payload_cache_bytes = std::nullopt,
                             .minimum_cache_footprint_capacity =
                                 cache_capacity_analysis(std::nullopt, abi.memory_facts()),
                             .page_bytes = abi.memory_facts().page_bytes,
                             .minimum_pages_touched = std::uint64_t{},
                             .minimum_page_bytes_touched = std::nullopt,
                             .minimum_read_pages_touched = std::uint64_t{},
                             .minimum_read_page_bytes_touched = std::nullopt,
                             .minimum_write_pages_touched = std::uint64_t{},
                             .minimum_write_page_bytes_touched = std::nullopt,
                             .non_payload_page_bytes = std::nullopt,
                             .diagnostics = {}};
    if (multiplicity == 0) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Access multiplicity must be non-zero."});
    }
    if (!result.cache_line_bytes.has_value() || *result.cache_line_bytes == 0) {
        result.minimum_cache_lines_touched.reset();
        result.minimum_read_cache_lines_touched.reset();
        result.minimum_write_cache_lines_touched.reset();
    }
    if (!result.page_bytes.has_value() || *result.page_bytes == 0) {
        result.minimum_pages_touched.reset();
        result.minimum_read_pages_touched.reset();
        result.minimum_write_pages_touched.reset();
    }
    if (soa.bytes_per_logical_element.has_value()) {
        result.full_logical_payload_bytes =
            checked_multiply(*soa.bytes_per_logical_element, element_count);
        if (!result.full_logical_payload_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Complete SoA logical payload at the selected count overflows uint64."});
        }
    }
    if (element_count > soa.capacity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Selected access count exceeds the modeled SoA allocation capacity."});
    }
    if (result.allocated_capacity_payload_bytes.has_value() &&
        result.full_logical_payload_bytes.has_value() &&
        *result.allocated_capacity_payload_bytes >= *result.full_logical_payload_bytes) {
        result.capacity_slack_payload_bytes =
            *result.allocated_capacity_payload_bytes - *result.full_logical_payload_bytes;
    }
    std::set<std::string, std::less<>> unique_names;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> accessed_intervals;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> read_intervals;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> write_intervals;
    for (auto const& access : accesses) {
        auto const& column_name{access.name};
        if (!unique_names.insert(column_name).second) {
            auto const existing{
                std::ranges::find(result.accesses, column_name, &AccessIntent::name)};
            if (existing != result.accesses.end() && existing->operation != access.operation) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA column '" + column_name +
                         "' has conflicting access classifications; the first is used."});
            }
            continue;
        }
        auto const column{std::ranges::find(soa.columns, column_name, &SoaColumnAnalysis::name)};
        if (column == soa.columns.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name + "' no longer exists."});
            continue;
        }
        result.column_names.push_back(column_name);
        result.accesses.push_back(access);

        auto accumulate = [&](std::optional<std::uint64_t> const value,
                              std::optional<std::uint64_t>& total,
                              std::string_view const fact) {
            if (!value.has_value() || !total.has_value()) {
                total.reset();
                if (!value.has_value()) {
                    result.diagnostics.push_back({DiagnosticSeverity::warning,
                                                  "Selected SoA column '" + column_name +
                                                      "' has Unknown " + std::string{fact} + "."});
                }
                return;
            }
            total = checked_add(*total, *value);
            if (!total.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA " + std::string{fact} + " total overflows uint64."});
            }
        };
        std::optional<std::uint64_t> flat_accessed_bytes;
        if (column->type_facts.has_value()) {
            flat_accessed_bytes = checked_multiply(column->type_facts->size_bytes, element_count);
            if (!flat_accessed_bytes.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Selected SoA column '" + column_name +
                                                  "' payload byte count overflows uint64."});
            }
        }
        SoaColumnAccessAnalysis column_access{
            .name = column_name,
            .operation = access.operation,
            .physical_type = column->physical_type,
            .element_bytes = column->type_facts.transform(
                [](TypeFacts const& facts) { return facts.size_bytes; }),
            .useful_bytes = flat_accessed_bytes,
            .read_useful_bytes = classified_useful_bytes(
                flat_accessed_bytes, access.operation != AccessOperation::write),
            .write_useful_bytes = classified_useful_bytes(
                flat_accessed_bytes, access.operation != AccessOperation::read),
            .logical_read_useful_bytes = logical_useful_bytes(
                classified_useful_bytes(flat_accessed_bytes,
                                        access.operation != AccessOperation::write),
                multiplicity),
            .logical_write_useful_bytes = logical_useful_bytes(
                classified_useful_bytes(flat_accessed_bytes,
                                        access.operation != AccessOperation::read),
                multiplicity),
            .minimum_cache_lines = std::nullopt,
            .minimum_cache_bytes = std::nullopt,
            .non_payload_cache_bytes = std::nullopt,
            .aligned_cache_line_straddling_elements = std::nullopt,
            .minimum_pages = std::nullopt,
            .minimum_page_bytes = std::nullopt,
            .non_payload_page_bytes = std::nullopt,
            .aligned_page_straddling_elements = std::nullopt,
            .allocated_capacity_payload_bytes = column->total_bytes,
            .capacity_slack_payload_bytes = std::nullopt};
        if (multiplicity != 0 && column_access.read_useful_bytes.has_value() &&
            !column_access.logical_read_useful_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name +
                     "' logical read useful byte total overflows uint64."});
        }
        if (multiplicity != 0 && column_access.write_useful_bytes.has_value() &&
            !column_access.logical_write_useful_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA column '" + column_name +
                     "' logical write useful byte total overflows uint64."});
        }
        if (column_access.allocated_capacity_payload_bytes.has_value() &&
            column_access.useful_bytes.has_value() &&
            *column_access.allocated_capacity_payload_bytes >= *column_access.useful_bytes) {
            column_access.capacity_slack_payload_bytes =
                *column_access.allocated_capacity_payload_bytes - *column_access.useful_bytes;
        }
        auto derive_column_regions = [&](std::optional<std::uint64_t> const region_size,
                                         std::optional<std::uint64_t>& regions,
                                         std::optional<std::uint64_t>& bytes,
                                         std::optional<std::uint64_t>& non_payload_bytes,
                                         std::string_view const region_name) {
            if (!region_size.has_value() || *region_size == 0 ||
                !column_access.useful_bytes.has_value()) {
                return;
            }
            regions = minimum_regions(*column_access.useful_bytes, *region_size);
            bytes = checked_multiply(*regions, *region_size);
            if (!bytes.has_value()) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Selected SoA column '" + column_name + "' minimum " +
                                                  std::string{region_name} +
                                                  " footprint overflows uint64."});
            } else {
                non_payload_bytes = *bytes - *column_access.useful_bytes;
            }
        };
        derive_column_regions(result.cache_line_bytes,
                              column_access.minimum_cache_lines,
                              column_access.minimum_cache_bytes,
                              column_access.non_payload_cache_bytes,
                              "cache-line");
        derive_column_regions(result.page_bytes,
                              column_access.minimum_pages,
                              column_access.minimum_page_bytes,
                              column_access.non_payload_page_bytes,
                              "page");
        if (access.operation != AccessOperation::write) {
            accumulate(column_access.minimum_cache_lines,
                       result.minimum_read_cache_lines_touched,
                       "minimum read cache-line count");
            accumulate(column_access.minimum_pages,
                       result.minimum_read_pages_touched,
                       "minimum read page count");
        }
        if (access.operation != AccessOperation::read) {
            accumulate(column_access.minimum_cache_lines,
                       result.minimum_write_cache_lines_touched,
                       "minimum write cache-line count");
            accumulate(column_access.minimum_pages,
                       result.minimum_write_pages_touched,
                       "minimum write page count");
        }
        if (soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
            column->allocation_offset_bytes.has_value() && flat_accessed_bytes.has_value() &&
            *flat_accessed_bytes != 0) {
            auto const interval{std::pair{*column->allocation_offset_bytes, *flat_accessed_bytes}};
            accessed_intervals.push_back(interval);
            if (access.operation != AccessOperation::write) {
                read_intervals.push_back(interval);
            }
            if (access.operation != AccessOperation::read) {
                write_intervals.push_back(interval);
            }
        }
        if (column_access.element_bytes.has_value()) {
            if (*column_access.element_bytes == 0) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA column '" + column_name + "' has zero-sized elements."});
            } else {
                if (result.cache_line_bytes.has_value() && *result.cache_line_bytes != 0) {
                    column_access.aligned_cache_line_straddling_elements =
                        soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
                                column->allocation_offset_bytes.has_value()
                            ? straddling_elements_at_offset(*column_access.element_bytes,
                                                            element_count,
                                                            *result.cache_line_bytes,
                                                            column_access.useful_bytes,
                                                            *column->allocation_offset_bytes)
                            : straddling_elements(*column_access.element_bytes,
                                                  element_count,
                                                  *result.cache_line_bytes,
                                                  column_access.useful_bytes);
                }
                if (result.page_bytes.has_value() && *result.page_bytes != 0) {
                    column_access.aligned_page_straddling_elements =
                        soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous &&
                                column->allocation_offset_bytes.has_value()
                            ? straddling_elements_at_offset(*column_access.element_bytes,
                                                            element_count,
                                                            *result.page_bytes,
                                                            column_access.useful_bytes,
                                                            *column->allocation_offset_bytes)
                            : straddling_elements(*column_access.element_bytes,
                                                  element_count,
                                                  *result.page_bytes,
                                                  column_access.useful_bytes);
                }
            }
        }
        accumulate(column_access.read_useful_bytes, result.read_useful_bytes, "read useful bytes");
        accumulate(
            column_access.write_useful_bytes, result.write_useful_bytes, "write useful bytes");
        result.columns.push_back(std::move(column_access));
        if (flat_accessed_bytes.has_value()) {
            accumulate(flat_accessed_bytes, result.useful_bytes, "payload bytes");
        } else {
            result.useful_bytes.reset();
            if (!column->type_facts.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Selected SoA column '" + column_name + "' has Unknown payload bytes."});
            }
        }
        if (result.minimum_cache_lines_touched.has_value()) {
            auto const cache_lines{flat_accessed_bytes.transform([&](std::uint64_t const bytes) {
                return minimum_regions(bytes, *result.cache_line_bytes);
            })};
            accumulate(cache_lines, result.minimum_cache_lines_touched, "minimum cache-line count");
        }
        if (result.minimum_pages_touched.has_value()) {
            auto const pages{flat_accessed_bytes.transform([&](std::uint64_t const bytes) {
                return minimum_regions(bytes, *result.page_bytes);
            })};
            accumulate(pages, result.minimum_pages_touched, "minimum page count");
        }
    }

    if (result.column_names.empty()) {
        result.useful_bytes.reset();
        result.read_useful_bytes.reset();
        result.write_useful_bytes.reset();
        result.logical_read_useful_bytes =
            logical_useful_bytes(result.read_useful_bytes, multiplicity);
        result.logical_write_useful_bytes =
            logical_useful_bytes(result.write_useful_bytes, multiplicity);
        result.minimum_cache_lines_touched.reset();
        result.minimum_read_cache_lines_touched.reset();
        result.minimum_write_cache_lines_touched.reset();
        result.minimum_pages_touched.reset();
        result.minimum_read_pages_touched.reset();
        result.minimum_write_pages_touched.reset();
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No SoA columns are selected for access."});
        }
        return result;
    }

    if (std::ranges::none_of(result.accesses, [](AccessIntent const& access) {
            return access.operation != AccessOperation::write;
        })) {
        result.minimum_read_cache_lines_touched = 0;
        result.minimum_read_cache_bytes_touched = 0;
        result.minimum_read_pages_touched = 0;
        result.minimum_read_page_bytes_touched = 0;
    }
    if (std::ranges::none_of(result.accesses, [](AccessIntent const& access) {
            return access.operation != AccessOperation::read;
        })) {
        result.minimum_write_cache_lines_touched = 0;
        result.minimum_write_cache_bytes_touched = 0;
        result.minimum_write_pages_touched = 0;
        result.minimum_write_page_bytes_touched = 0;
    }

    if (result.full_logical_payload_bytes.has_value() && result.useful_bytes.has_value()) {
        if (*result.full_logical_payload_bytes >= *result.useful_bytes) {
            result.unselected_payload_bytes =
                *result.full_logical_payload_bytes - *result.useful_bytes;
        } else {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Selected SoA payload bytes exceed the complete logical payload."});
        }
    }
    if (soa.allocation_strategy == SoaAllocationStrategy::aligned_contiguous) {
        if (element_count > soa.capacity || !soa.total_allocation_bytes.has_value()) {
            result.footprint_exact = false;
            result.minimum_cache_lines_touched.reset();
            result.minimum_read_cache_lines_touched.reset();
            result.minimum_write_cache_lines_touched.reset();
            result.minimum_pages_touched.reset();
            result.minimum_read_pages_touched.reset();
            result.minimum_write_pages_touched.reset();
        } else {
            auto exact_regions = [&](auto& intervals,
                                     std::optional<std::uint64_t> const region_size,
                                     std::optional<std::uint64_t>& regions,
                                     char const* const region_name) {
                if (element_count == 0 || intervals.empty()) {
                    regions = 0;
                    return;
                }
                if (!region_size.has_value() || *region_size == 0) {
                    regions.reset();
                    return;
                }
                std::ranges::sort(intervals);
                std::string error;
                regions = touched_regions(0, intervals, 1, *region_size, error);
                if (!regions.has_value()) {
                    result.diagnostics.push_back(
                        {DiagnosticSeverity::error,
                         std::string{"Contiguous SoA "} + region_name + ": " + error});
                }
            };
            exact_regions(accessed_intervals,
                          result.cache_line_bytes,
                          result.minimum_cache_lines_touched,
                          "cache-line access analysis");
            exact_regions(read_intervals,
                          result.cache_line_bytes,
                          result.minimum_read_cache_lines_touched,
                          "read cache-line access analysis");
            exact_regions(write_intervals,
                          result.cache_line_bytes,
                          result.minimum_write_cache_lines_touched,
                          "write cache-line access analysis");
            exact_regions(accessed_intervals,
                          result.page_bytes,
                          result.minimum_pages_touched,
                          "page access analysis");
            exact_regions(read_intervals,
                          result.page_bytes,
                          result.minimum_read_pages_touched,
                          "read page access analysis");
            exact_regions(write_intervals,
                          result.page_bytes,
                          result.minimum_write_pages_touched,
                          "write page access analysis");
        }
    }
    auto derive_region_bytes = [&](std::optional<std::uint64_t>& region_size,
                                   std::optional<std::uint64_t> const regions,
                                   std::optional<std::uint64_t>& touched_bytes,
                                   std::optional<std::uint64_t>& non_payload_bytes,
                                   char const* const region_name) {
        if (!region_size.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          std::string{region_name} +
                                              " size is unknown for ABI profile '" + abi.name() +
                                              "'."});
            return;
        }
        if (*region_size == 0) {
            region_size.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, std::string{region_name} + " size must be non-zero."});
            return;
        }
        if (!regions.has_value()) {
            return;
        }
        touched_bytes = checked_multiply(*regions, *region_size);
        if (!touched_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Selected SoA minimum " + std::string{region_name} +
                                              " footprint overflows uint64."});
            return;
        }
        if (result.useful_bytes.has_value()) {
            if (*touched_bytes >= *result.useful_bytes) {
                non_payload_bytes = *touched_bytes - *result.useful_bytes;
            } else {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Selected SoA minimum " + std::string{region_name} +
                         " footprint is smaller than useful payload bytes."});
            }
        }
    };
    derive_region_bytes(result.cache_line_bytes,
                        result.minimum_cache_lines_touched,
                        result.minimum_cache_bytes_touched,
                        result.non_payload_cache_bytes,
                        "cache-line");
    result.minimum_cache_footprint_capacity =
        cache_capacity_analysis(result.minimum_cache_bytes_touched, abi.memory_facts());
    derive_region_bytes(result.page_bytes,
                        result.minimum_pages_touched,
                        result.minimum_page_bytes_touched,
                        result.non_payload_page_bytes,
                        "page");
    auto derive_classified_region_bytes = [&](std::optional<std::uint64_t> const region_size,
                                              std::optional<std::uint64_t> const regions,
                                              std::optional<std::uint64_t>& bytes,
                                              char const* const category) {
        if (!region_size.has_value() || !regions.has_value()) {
            return;
        }
        bytes = checked_multiply(*regions, *region_size);
        if (!bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 std::string{category} + " address-coverage byte count overflows uint64."});
        }
    };
    derive_classified_region_bytes(result.cache_line_bytes,
                                   result.minimum_read_cache_lines_touched,
                                   result.minimum_read_cache_bytes_touched,
                                   "Minimum read cache-line");
    derive_classified_region_bytes(result.cache_line_bytes,
                                   result.minimum_write_cache_lines_touched,
                                   result.minimum_write_cache_bytes_touched,
                                   "Minimum write cache-line");
    derive_classified_region_bytes(result.page_bytes,
                                   result.minimum_read_pages_touched,
                                   result.minimum_read_page_bytes_touched,
                                   "Minimum read page");
    derive_classified_region_bytes(result.page_bytes,
                                   result.minimum_write_pages_touched,
                                   result.minimum_write_page_bytes_touched,
                                   "Minimum write page");
    result.logical_read_useful_bytes = logical_useful_bytes(result.read_useful_bytes, multiplicity);
    result.logical_write_useful_bytes =
        logical_useful_bytes(result.write_useful_bytes, multiplicity);
    if (multiplicity != 0 && result.read_useful_bytes.has_value() &&
        !result.logical_read_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical read useful byte total overflows uint64."});
    }
    if (multiplicity != 0 && result.write_useful_bytes.has_value() &&
        !result.logical_write_useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Logical write useful byte total overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_soa_access(SoaAnalysis const& soa,
                                  std::span<std::string const> const column_names,
                                  AbiProfile const& abi,
                                  std::uint64_t const element_count,
                                  AccessOperation const operation,
                                  std::uint64_t const multiplicity) -> SoaAccessAnalysis {
    std::vector<AccessIntent> accesses;
    accesses.reserve(column_names.size());
    for (auto const& column_name : column_names) {
        accesses.push_back({.name = column_name, .operation = operation});
    }
    return analyze_soa_access(soa, accesses, abi, element_count, multiplicity);
}

auto Analyzer::compare_record_soa_access(RecordAccessAnalysis const& record,
                                         SoaAccessAnalysis const& soa)
    -> RecordSoaAccessComparison {
    RecordSoaAccessComparison result{
        .member_names = soa.column_names,
        .accesses = soa.accesses,
        .element_count = soa.element_count,
        .multiplicity = record.multiplicity,
        .soa_allocation_strategy = soa.allocation_strategy,
        .soa_footprint_exact = soa.footprint_exact,
        .record = {.useful_bytes = record.useful_bytes,
                   .read_useful_bytes = record.read_useful_bytes,
                   .write_useful_bytes = record.write_useful_bytes,
                   .logical_read_useful_bytes = record.logical_read_useful_bytes,
                   .logical_write_useful_bytes = record.logical_write_useful_bytes,
                   .cache_lines = record.cache_lines_touched,
                   .cache_bytes = record.cache_bytes_touched,
                   .read_cache_lines = record.read_cache_lines_touched,
                   .read_cache_bytes = record.read_cache_bytes_touched,
                   .write_cache_lines = record.write_cache_lines_touched,
                   .write_cache_bytes = record.write_cache_bytes_touched,
                   .pages = record.pages_touched,
                   .page_bytes = record.page_bytes_touched,
                   .read_pages = record.read_pages_touched,
                   .read_page_bytes = record.read_page_bytes_touched,
                   .write_pages = record.write_pages_touched,
                   .write_page_bytes = record.write_page_bytes_touched},
        .soa = {.useful_bytes = soa.useful_bytes,
                .read_useful_bytes = soa.read_useful_bytes,
                .write_useful_bytes = soa.write_useful_bytes,
                .logical_read_useful_bytes = soa.logical_read_useful_bytes,
                .logical_write_useful_bytes = soa.logical_write_useful_bytes,
                .cache_lines = soa.minimum_cache_lines_touched,
                .cache_bytes = soa.minimum_cache_bytes_touched,
                .read_cache_lines = soa.minimum_read_cache_lines_touched,
                .read_cache_bytes = soa.minimum_read_cache_bytes_touched,
                .write_cache_lines = soa.minimum_write_cache_lines_touched,
                .write_cache_bytes = soa.minimum_write_cache_bytes_touched,
                .pages = soa.minimum_pages_touched,
                .page_bytes = soa.minimum_page_bytes_touched,
                .read_pages = soa.minimum_read_pages_touched,
                .read_page_bytes = soa.minimum_read_page_bytes_touched,
                .write_pages = soa.minimum_write_pages_touched,
                .write_page_bytes = soa.minimum_write_page_bytes_touched},
        .record_cache_footprint_capacity = record.cache_footprint_capacity,
        .soa_minimum_cache_footprint_capacity = soa.minimum_cache_footprint_capacity,
        .record_non_useful_cache_bytes = record.non_selected_cache_bytes,
        .soa_non_useful_cache_bytes = soa.non_payload_cache_bytes,
        .record_non_useful_page_bytes = record.non_selected_page_bytes,
        .soa_non_useful_page_bytes = soa.non_payload_page_bytes,
        .useful_byte_delta = std::nullopt,
        .read_useful_byte_delta = std::nullopt,
        .write_useful_byte_delta = std::nullopt,
        .logical_read_useful_byte_delta = std::nullopt,
        .logical_write_useful_byte_delta = std::nullopt,
        .cache_line_delta = std::nullopt,
        .cache_byte_delta = std::nullopt,
        .read_cache_line_delta = std::nullopt,
        .read_cache_byte_delta = std::nullopt,
        .write_cache_line_delta = std::nullopt,
        .write_cache_byte_delta = std::nullopt,
        .page_delta = std::nullopt,
        .page_byte_delta = std::nullopt,
        .read_page_delta = std::nullopt,
        .read_page_byte_delta = std::nullopt,
        .write_page_delta = std::nullopt,
        .write_page_byte_delta = std::nullopt,
        .non_useful_cache_byte_delta = std::nullopt,
        .non_useful_page_byte_delta = std::nullopt,
        .diagnostics = {}};
    for (auto const& diagnostic : record.diagnostics) {
        result.diagnostics.push_back({diagnostic.severity, "AoS access: " + diagnostic.message});
    }
    for (auto const& diagnostic : soa.diagnostics) {
        result.diagnostics.push_back({diagnostic.severity, "SoA access: " + diagnostic.message});
    }
    if (record.element_count != soa.element_count) {
        result.diagnostics.push_back({DiagnosticSeverity::error,
                                      "AoS and SoA access analyses use different element counts."});
        return result;
    }
    if (!access_intents_match(record.accesses, soa.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses use different per-field access classifications."});
        return result;
    }
    if (record.multiplicity != soa.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses use different access multiplicities."});
        return result;
    }
    auto const record_names{
        std::set<std::string, std::less<>>{record.member_names.begin(), record.member_names.end()}};
    auto const soa_names{
        std::set<std::string, std::less<>>{soa.column_names.begin(), soa.column_names.end()}};
    if (record_names != soa_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "AoS and SoA access analyses do not select the same named fields."});
        return result;
    }
    result.useful_byte_delta = numeric_delta(result.record.useful_bytes, result.soa.useful_bytes);
    result.read_useful_byte_delta =
        numeric_delta(result.record.read_useful_bytes, result.soa.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(result.record.write_useful_bytes, result.soa.write_useful_bytes);
    result.logical_read_useful_byte_delta = numeric_delta(result.record.logical_read_useful_bytes,
                                                          result.soa.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta = numeric_delta(result.record.logical_write_useful_bytes,
                                                           result.soa.logical_write_useful_bytes);
    result.cache_line_delta = numeric_delta(result.record.cache_lines, result.soa.cache_lines);
    result.cache_byte_delta = numeric_delta(result.record.cache_bytes, result.soa.cache_bytes);
    result.read_cache_line_delta =
        numeric_delta(result.record.read_cache_lines, result.soa.read_cache_lines);
    result.read_cache_byte_delta =
        numeric_delta(result.record.read_cache_bytes, result.soa.read_cache_bytes);
    result.write_cache_line_delta =
        numeric_delta(result.record.write_cache_lines, result.soa.write_cache_lines);
    result.write_cache_byte_delta =
        numeric_delta(result.record.write_cache_bytes, result.soa.write_cache_bytes);
    result.page_delta = numeric_delta(result.record.pages, result.soa.pages);
    result.page_byte_delta = numeric_delta(result.record.page_bytes, result.soa.page_bytes);
    result.read_page_delta = numeric_delta(result.record.read_pages, result.soa.read_pages);
    result.read_page_byte_delta =
        numeric_delta(result.record.read_page_bytes, result.soa.read_page_bytes);
    result.write_page_delta = numeric_delta(result.record.write_pages, result.soa.write_pages);
    result.write_page_byte_delta =
        numeric_delta(result.record.write_page_bytes, result.soa.write_page_bytes);
    result.non_useful_cache_byte_delta =
        numeric_delta(result.record_non_useful_cache_bytes, result.soa_non_useful_cache_bytes);
    result.non_useful_page_byte_delta =
        numeric_delta(result.record_non_useful_page_bytes, result.soa_non_useful_page_bytes);
    return result;
}

auto Analyzer::compare_soa_access(SoaAccessAnalysis const& first, SoaAccessAnalysis const& second)
    -> SoaAccessComparison {
    auto summary = [](SoaAccessAnalysis const& analysis) {
        return AccessFootprintSummary{
            .useful_bytes = analysis.useful_bytes,
            .read_useful_bytes = analysis.read_useful_bytes,
            .write_useful_bytes = analysis.write_useful_bytes,
            .logical_read_useful_bytes = analysis.logical_read_useful_bytes,
            .logical_write_useful_bytes = analysis.logical_write_useful_bytes,
            .cache_lines = analysis.minimum_cache_lines_touched,
            .cache_bytes = analysis.minimum_cache_bytes_touched,
            .read_cache_lines = analysis.minimum_read_cache_lines_touched,
            .read_cache_bytes = analysis.minimum_read_cache_bytes_touched,
            .write_cache_lines = analysis.minimum_write_cache_lines_touched,
            .write_cache_bytes = analysis.minimum_write_cache_bytes_touched,
            .pages = analysis.minimum_pages_touched,
            .page_bytes = analysis.minimum_page_bytes_touched,
            .read_pages = analysis.minimum_read_pages_touched,
            .read_page_bytes = analysis.minimum_read_page_bytes_touched,
            .write_pages = analysis.minimum_write_pages_touched,
            .write_page_bytes = analysis.minimum_write_page_bytes_touched};
    };
    SoaAccessComparison result{
        .column_names = first.column_names,
        .accesses = first.accesses,
        .columns = {},
        .element_count = first.element_count,
        .multiplicity = first.multiplicity,
        .allocation_strategy = first.allocation_strategy,
        .footprint_exact = first.footprint_exact && second.footprint_exact,
        .first = summary(first),
        .second = summary(second),
        .first_allocation_count = first.allocation_count,
        .second_allocation_count = second.allocation_count,
        .first_total_allocation_bytes = first.total_allocation_bytes,
        .second_total_allocation_bytes = second.total_allocation_bytes,
        .first_alignment_padding_bytes = first.alignment_padding_bytes,
        .second_alignment_padding_bytes = second.alignment_padding_bytes,
        .allocation_count_delta = std::nullopt,
        .total_allocation_byte_delta = std::nullopt,
        .alignment_padding_byte_delta = std::nullopt,
        .first_minimum_cache_footprint_capacity = first.minimum_cache_footprint_capacity,
        .second_minimum_cache_footprint_capacity = second.minimum_cache_footprint_capacity,
        .first_allocated_capacity_payload_bytes = first.allocated_capacity_payload_bytes,
        .second_allocated_capacity_payload_bytes = second.allocated_capacity_payload_bytes,
        .first_capacity_slack_payload_bytes = first.capacity_slack_payload_bytes,
        .second_capacity_slack_payload_bytes = second.capacity_slack_payload_bytes,
        .first_full_logical_payload_bytes = first.full_logical_payload_bytes,
        .second_full_logical_payload_bytes = second.full_logical_payload_bytes,
        .first_unselected_payload_bytes = first.unselected_payload_bytes,
        .second_unselected_payload_bytes = second.unselected_payload_bytes,
        .first_non_payload_cache_bytes = first.non_payload_cache_bytes,
        .second_non_payload_cache_bytes = second.non_payload_cache_bytes,
        .first_non_payload_page_bytes = first.non_payload_page_bytes,
        .second_non_payload_page_bytes = second.non_payload_page_bytes,
        .useful_byte_delta = std::nullopt,
        .read_useful_byte_delta = std::nullopt,
        .write_useful_byte_delta = std::nullopt,
        .logical_read_useful_byte_delta = std::nullopt,
        .logical_write_useful_byte_delta = std::nullopt,
        .cache_line_delta = std::nullopt,
        .cache_byte_delta = std::nullopt,
        .read_cache_line_delta = std::nullopt,
        .read_cache_byte_delta = std::nullopt,
        .write_cache_line_delta = std::nullopt,
        .write_cache_byte_delta = std::nullopt,
        .page_delta = std::nullopt,
        .page_byte_delta = std::nullopt,
        .read_page_delta = std::nullopt,
        .read_page_byte_delta = std::nullopt,
        .write_page_delta = std::nullopt,
        .write_page_byte_delta = std::nullopt,
        .allocated_capacity_payload_delta = std::nullopt,
        .capacity_slack_payload_delta = std::nullopt,
        .full_logical_payload_delta = std::nullopt,
        .unselected_payload_delta = std::nullopt,
        .non_payload_cache_byte_delta = std::nullopt,
        .non_payload_page_byte_delta = std::nullopt,
        .diagnostics = {}};
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First SoA access: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second SoA access: " + diagnostic.message});
    }
    if (first.element_count != second.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different element counts."});
        return result;
    }
    if (first.allocation_strategy != second.allocation_strategy) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different allocation strategies."});
        return result;
    }
    if (!access_intents_match(first.accesses, second.accesses)) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different per-column access classifications."});
        return result;
    }
    if (first.multiplicity != second.multiplicity) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses use different access multiplicities."});
        return result;
    }
    auto const first_names{
        std::set<std::string, std::less<>>{first.column_names.begin(), first.column_names.end()}};
    auto const second_names{
        std::set<std::string, std::less<>>{second.column_names.begin(), second.column_names.end()}};
    if (first_names.size() != first.column_names.size() ||
        second_names.size() != second.column_names.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses do not select the same unique named columns."});
        return result;
    }
    auto detail_names = [](std::span<SoaColumnAccessAnalysis const> const columns) {
        std::set<std::string, std::less<>> names;
        for (auto const& column : columns) {
            if (!names.insert(column.name).second) {
                return std::optional<std::set<std::string, std::less<>>>{};
            }
        }
        return std::optional{std::move(names)};
    };
    auto const first_detail_names{detail_names(first.columns)};
    auto const second_detail_names{detail_names(second.columns)};
    if (!first_detail_names.has_value() || !second_detail_names.has_value() ||
        *first_detail_names != first_names || *second_detail_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared SoA access analyses do not contain exactly one detail for every selected "
             "column."});
        return result;
    }
    for (auto const& column_name : result.column_names) {
        auto const first_column{
            std::ranges::find(first.columns, column_name, &SoaColumnAccessAnalysis::name)};
        auto const second_column{
            std::ranges::find(second.columns, column_name, &SoaColumnAccessAnalysis::name)};
        if (first_column == first.columns.end() || second_column == second.columns.end()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared SoA access analysis is missing details for selected column '" +
                     column_name + "'."});
            return result;
        }
        if (first_column->operation != second_column->operation) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared SoA access analyses classify selected column '" + column_name +
                     "' differently."});
            return result;
        }
    }
    for (auto const& column_name : result.column_names) {
        auto const first_column{
            std::ranges::find(first.columns, column_name, &SoaColumnAccessAnalysis::name)};
        auto const second_column{
            std::ranges::find(second.columns, column_name, &SoaColumnAccessAnalysis::name)};
        result.columns.push_back(
            {.name = column_name,
             .first = *first_column,
             .second = *second_column,
             .element_byte_delta =
                 numeric_delta(first_column->element_bytes, second_column->element_bytes),
             .useful_byte_delta =
                 numeric_delta(first_column->useful_bytes, second_column->useful_bytes),
             .read_useful_byte_delta =
                 numeric_delta(first_column->read_useful_bytes, second_column->read_useful_bytes),
             .write_useful_byte_delta =
                 numeric_delta(first_column->write_useful_bytes, second_column->write_useful_bytes),
             .logical_read_useful_byte_delta = numeric_delta(
                 first_column->logical_read_useful_bytes, second_column->logical_read_useful_bytes),
             .logical_write_useful_byte_delta =
                 numeric_delta(first_column->logical_write_useful_bytes,
                               second_column->logical_write_useful_bytes),
             .cache_line_delta = numeric_delta(first_column->minimum_cache_lines,
                                               second_column->minimum_cache_lines),
             .cache_byte_delta = numeric_delta(first_column->minimum_cache_bytes,
                                               second_column->minimum_cache_bytes),
             .non_payload_cache_byte_delta = numeric_delta(first_column->non_payload_cache_bytes,
                                                           second_column->non_payload_cache_bytes),
             .aligned_cache_line_straddling_element_delta =
                 numeric_delta(first_column->aligned_cache_line_straddling_elements,
                               second_column->aligned_cache_line_straddling_elements),
             .page_delta = numeric_delta(first_column->minimum_pages, second_column->minimum_pages),
             .page_byte_delta =
                 numeric_delta(first_column->minimum_page_bytes, second_column->minimum_page_bytes),
             .non_payload_page_byte_delta = numeric_delta(first_column->non_payload_page_bytes,
                                                          second_column->non_payload_page_bytes),
             .aligned_page_straddling_element_delta =
                 numeric_delta(first_column->aligned_page_straddling_elements,
                               second_column->aligned_page_straddling_elements),
             .allocated_capacity_payload_delta =
                 numeric_delta(first_column->allocated_capacity_payload_bytes,
                               second_column->allocated_capacity_payload_bytes),
             .capacity_slack_payload_delta =
                 numeric_delta(first_column->capacity_slack_payload_bytes,
                               second_column->capacity_slack_payload_bytes)});
    }
    result.useful_byte_delta = numeric_delta(result.first.useful_bytes, result.second.useful_bytes);
    result.allocation_count_delta = numeric_delta(first.allocation_count, second.allocation_count);
    result.total_allocation_byte_delta =
        numeric_delta(result.first_total_allocation_bytes, result.second_total_allocation_bytes);
    result.alignment_padding_byte_delta =
        numeric_delta(result.first_alignment_padding_bytes, result.second_alignment_padding_bytes);
    result.read_useful_byte_delta =
        numeric_delta(result.first.read_useful_bytes, result.second.read_useful_bytes);
    result.write_useful_byte_delta =
        numeric_delta(result.first.write_useful_bytes, result.second.write_useful_bytes);
    result.logical_read_useful_byte_delta = numeric_delta(result.first.logical_read_useful_bytes,
                                                          result.second.logical_read_useful_bytes);
    result.logical_write_useful_byte_delta = numeric_delta(
        result.first.logical_write_useful_bytes, result.second.logical_write_useful_bytes);
    result.cache_line_delta = numeric_delta(result.first.cache_lines, result.second.cache_lines);
    result.cache_byte_delta = numeric_delta(result.first.cache_bytes, result.second.cache_bytes);
    result.read_cache_line_delta =
        numeric_delta(result.first.read_cache_lines, result.second.read_cache_lines);
    result.read_cache_byte_delta =
        numeric_delta(result.first.read_cache_bytes, result.second.read_cache_bytes);
    result.write_cache_line_delta =
        numeric_delta(result.first.write_cache_lines, result.second.write_cache_lines);
    result.write_cache_byte_delta =
        numeric_delta(result.first.write_cache_bytes, result.second.write_cache_bytes);
    result.page_delta = numeric_delta(result.first.pages, result.second.pages);
    result.page_byte_delta = numeric_delta(result.first.page_bytes, result.second.page_bytes);
    result.read_page_delta = numeric_delta(result.first.read_pages, result.second.read_pages);
    result.read_page_byte_delta =
        numeric_delta(result.first.read_page_bytes, result.second.read_page_bytes);
    result.write_page_delta = numeric_delta(result.first.write_pages, result.second.write_pages);
    result.write_page_byte_delta =
        numeric_delta(result.first.write_page_bytes, result.second.write_page_bytes);
    result.allocated_capacity_payload_delta =
        numeric_delta(result.first_allocated_capacity_payload_bytes,
                      result.second_allocated_capacity_payload_bytes);
    result.capacity_slack_payload_delta = numeric_delta(result.first_capacity_slack_payload_bytes,
                                                        result.second_capacity_slack_payload_bytes);
    result.full_logical_payload_delta = numeric_delta(result.first_full_logical_payload_bytes,
                                                      result.second_full_logical_payload_bytes);
    result.unselected_payload_delta = numeric_delta(result.first_unselected_payload_bytes,
                                                    result.second_unselected_payload_bytes);
    result.non_payload_cache_byte_delta =
        numeric_delta(result.first_non_payload_cache_bytes, result.second_non_payload_cache_bytes);
    result.non_payload_page_byte_delta =
        numeric_delta(result.first_non_payload_page_bytes, result.second_non_payload_page_bytes);
    return result;
}

} // namespace ioj::layout

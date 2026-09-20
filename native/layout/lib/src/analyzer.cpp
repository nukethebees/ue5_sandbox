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

auto straddling_elements(std::uint64_t const element_bytes,
                         std::uint64_t const element_count,
                         std::uint64_t const region_bytes,
                         std::optional<std::uint64_t> const total_bytes)
    -> std::optional<std::uint64_t> {
    if (element_count == 0) {
        return 0;
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
            current = enumeration->underlying_type.type;
            continue;
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
                            AbiProfile const& abi) -> EnumDomainAnalysis {
    auto const& enumeration{std::get<lispb::schema::EnumType>(types.type(type).definition)};
    auto const backing_type{physical_type_spelling(types, enumeration.underlying_type.type)
                                .value_or(enumeration.underlying_type.cpp_type.spelling)};
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
                              .diagnostics = {}};
    if (!result.backing_facts.has_value()) {
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

    std::vector<lispb::schema::EnumDomainInput> domain_inputs;
    domain_inputs.reserve(enumeration.enumerators.size());
    for (auto const& enumerator : enumeration.enumerators) {
        domain_inputs.push_back({.name = enumerator.name,
                                 .explicit_value = enumerator.explicit_value,
                                 .reserved = enumerator.sentinel || enumerator.count_sentinel});
    }
    auto const domain{lispb::schema::analyze_enum_domain(domain_inputs, enumeration.signedness)};
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
        if (result.backing_bits.has_value() && *result.backing_bits < 64) {
            auto const backing_codes{std::uint64_t{1} << *result.backing_bits};
            if (domain.distinct_code_count <= backing_codes) {
                result.unused_backing_codes = backing_codes - domain.distinct_code_count;
            }
        }
    }
    return result;
}

auto Analyzer::analyze_integer_scalar(lispb::schema::TypeGraph const& types,
                                      lispb::schema::TypeId const type) -> IntegerScalarAnalysis {
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

    return {.type = type,
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
            .named_codes = std::move(named_codes)};
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

    auto const numerical_code_count{std::ldexp(1.0L, static_cast<int>(quantized.bit_width))};
    auto const numerical_usable_count{numerical_code_count -
                                      static_cast<long double>(quantized.reserved_codes)};
    auto const numerical_span{packed_integer_as_long_double(source.maximum_value) -
                              packed_integer_as_long_double(source.minimum_value)};
    auto const resolution{numerical_span / (numerical_usable_count - 1.0L)};

    return {.type = type,
            .source_type = quantized.source.type,
            .source_minimum = source.minimum_value,
            .source_maximum = source.maximum_value,
            .source_span = packed_range_span(source.minimum_value, source.maximum_value),
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

auto Analyzer::analyze_record_access(RecordAnalysis const& record,
                                     std::span<std::string const> const member_names,
                                     AbiProfile const& abi) -> RecordAccessAnalysis {
    RecordAccessAnalysis result{.member_names = {},
                                .element_count = record.aggregate.element_count,
                                .useful_bytes = std::nullopt,
                                .object_footprint_bytes = record.aggregate.total_storage_bytes,
                                .cache_line_bytes = std::nullopt,
                                .cache_lines_touched = std::nullopt,
                                .cache_bytes_touched = std::nullopt,
                                .non_selected_cache_bytes = std::nullopt,
                                .page_bytes = std::nullopt,
                                .pages_touched = std::nullopt,
                                .diagnostics = {}};
    if (!record.size_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Record has incomplete target layout facts."});
        return result;
    }

    std::set<std::string, std::less<>> unique_names;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> member_layouts;
    std::uint64_t useful_bytes_per_element{};
    for (auto const& member_name : member_names) {
        if (!unique_names.insert(member_name).second) {
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
        if (!member->offset_bytes.has_value() || !member->extent_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Selected record member '" + member_name +
                                              "' has incomplete target layout facts."});
            continue;
        }
        member_layouts.emplace_back(*member->offset_bytes, *member->extent_bytes);
        auto const next{checked_add(useful_bytes_per_element, *member->extent_bytes)};
        if (!next.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Selected member byte count overflows uint64."});
            return result;
        }
        useful_bytes_per_element = *next;
    }
    if (member_layouts.empty()) {
        if (result.diagnostics.empty()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning, "No record members are selected for access."});
        }
        return result;
    }
    std::ranges::sort(member_layouts);
    result.useful_bytes = checked_multiply(useful_bytes_per_element, result.element_count);
    if (!result.useful_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Selected member useful byte count overflows uint64."});
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

    if (result.cache_lines_touched.has_value() && result.cache_line_bytes.has_value()) {
        result.cache_bytes_touched =
            checked_multiply(*result.cache_lines_touched, *result.cache_line_bytes);
        if (!result.cache_bytes_touched.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Touched cache-line byte count overflows uint64."});
        } else if (result.useful_bytes.has_value()) {
            if (*result.cache_bytes_touched >= *result.useful_bytes) {
                result.non_selected_cache_bytes =
                    *result.cache_bytes_touched - *result.useful_bytes;
            } else {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Touched cache-line bytes are smaller than useful member bytes."});
            }
        }
    }
    return result;
}

auto Analyzer::analyze_record_member_access(RecordAnalysis const& record,
                                            std::string_view const member_name,
                                            AbiProfile const& abi) -> RecordAccessAnalysis {
    auto const names{std::vector<std::string>{std::string{member_name}}};
    return analyze_record_access(record, names, abi);
}

auto Analyzer::analyze_packed(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              Variant const& variant,
                              AbiProfile const& abi,
                              std::uint64_t const element_count) -> PackedAnalysis {
    auto const& packed{std::get<lispb::schema::PackedType>(types.type(type).definition)};
    auto const schema_storage{physical_type_spelling(types, packed.storage_type.type)
                                  .value_or(packed.storage_type.cpp_type.spelling)};
    PackedAnalysis result{.type = type,
                          .schema_storage_type = schema_storage,
                          .storage_type = effective_storage_type(types, type, packed, variant),
                          .storage_overridden =
                              variant.overrides.packed_storage_types.contains(type),
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
                                        .page_bytes = std::nullopt,
                                        .minimum_pages = std::nullopt,
                                        .complete_elements_per_page = std::nullopt,
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
        auto const width{field != nullptr ? effective_field_width(type, *field, variant)
                                          : reserved->bit_width};
        auto const next_offset{checked_add(offset, width)};
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
            .overridden =
                field != nullptr && variant.overrides.packed_field_widths.contains(
                                        FieldOverrideId{.type = type, .field_name = field->name}),
            .least_significant_bit = offset,
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
            .relationship_kind = field != nullptr && field->relationship.has_value()
                                   ? std::optional{field->relationship->kind}
                                   : std::nullopt,
            .relationship_target =
                field != nullptr && field->relationship.has_value()
                    ? std::optional{types.type(field->relationship->target.type).identity.name}
                    : std::nullopt};
        if (width == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed segment '" + field_result.name + "' must use at least one bit."});
        }
        if (field != nullptr && !field_result.maximum_unsigned_value.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field->name + "' exceeds the 64-bit V1 range limit."});
        }
        std::optional<codegen::PackedIntegerValue> minimum_required_code;
        std::optional<codegen::PackedIntegerValue> maximum_required_code;
        if (field != nullptr) {
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
        if (next_offset.has_value()) {
            if (width != 0) {
                field_result.most_significant_bit = *next_offset - 1;
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

auto Analyzer::analyze_soa(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           Variant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const default_capacity) -> SoaAnalysis {
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
                       .columns = {},
                       .bytes_per_logical_element = std::nullopt,
                       .total_payload_bytes = std::nullopt,
                       .page_bytes = abi.memory_facts().page_bytes,
                       .minimum_pages = std::nullopt,
                       .cache_capacity = {},
                       .diagnostics = {}};
    auto const cache_line_bytes{abi.memory_facts().cache_line_bytes};
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
            .minimum_cache_lines = std::nullopt,
            .elements_per_cache_line = std::nullopt,
            .minimum_pages = std::nullopt,
            .complete_elements_per_page = std::nullopt,
            .cache_line_tiling = std::nullopt};
        column_result.type_facts = abi.find(column_result.physical_type);
        if (!column_result.type_facts.has_value()) {
            complete = false;
            pages_complete = false;
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Unknown physical facts for SoA column '" + column.name +
                                              "' type '" + column_result.physical_type + "'."});
            result.columns.push_back(std::move(column_result));
            continue;
        }

        auto const size{column_result.type_facts->size_bytes};
        if (size == 0) {
            complete = false;
            pages_complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "SoA column '" + column.name + "' has a zero-byte physical type."});
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
        result.columns.push_back(std::move(column_result));
    }

    if (complete) {
        result.bytes_per_logical_element = row_bytes;
        result.total_payload_bytes = total_bytes;
    }
    result.cache_capacity = cache_capacity_analysis(result.total_payload_bytes, abi.memory_facts());
    if (pages_complete) {
        result.minimum_pages = total_pages;
    }
    return result;
}

} // namespace ioj::layout

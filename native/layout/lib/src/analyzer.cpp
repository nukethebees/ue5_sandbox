#include <ioj/layout/analyzer.hpp>

#include <codegen/schema.h>

#include <algorithm>
#include <cctype>
#include <charconv>
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

auto parse_enum_code(std::string_view text) -> std::optional<EnumCodeValue> {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    bool negative{};
    if (text.starts_with('+') || text.starts_with('-')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    auto base{10};
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    while (!text.empty() &&
           (text.back() == 'u' || text.back() == 'U' || text.back() == 'l' || text.back() == 'L')) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::string digits;
    digits.reserve(text.size());
    for (auto const character : text) {
        if (character != '\'') {
            digits += character;
        }
    }
    std::uint64_t magnitude{};
    auto const [end, error]{
        std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base)};
    if (error != std::errc{} || end != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return EnumCodeValue{.negative = negative && magnitude != 0, .magnitude = magnitude};
}

auto next_enum_code(EnumCodeValue const value) -> std::optional<EnumCodeValue> {
    if (value.negative) {
        return value.magnitude == 1
                 ? std::optional{EnumCodeValue{.negative = false, .magnitude = 0}}
                 : std::optional{EnumCodeValue{.negative = true, .magnitude = value.magnitude - 1}};
    }
    if (value.magnitude == std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }
    return EnumCodeValue{.negative = false, .magnitude = value.magnitude + 1};
}

auto enum_code_less(EnumCodeValue const left, EnumCodeValue const right) -> bool {
    if (left.negative != right.negative) {
        return left.negative;
    }
    return left.negative ? left.magnitude > right.magnitude : left.magnitude < right.magnitude;
}

auto minimum_enum_bits(EnumCodeValue const minimum, EnumCodeValue const maximum)
    -> std::optional<std::uint32_t> {
    if (!minimum.negative) {
        auto bits{std::uint32_t{1}};
        auto remaining{maximum.magnitude};
        while (remaining > 1) {
            remaining >>= 1;
            ++bits;
        }
        return bits;
    }
    for (auto bits{std::uint32_t{1}}; bits <= 64; ++bits) {
        auto const magnitude_limit{bits == 64 ? (std::uint64_t{1} << 63)
                                              : (std::uint64_t{1} << (bits - 1))};
        auto const positive_limit{magnitude_limit - 1};
        if (minimum.magnitude <= magnitude_limit && !maximum.negative &&
            maximum.magnitude <= positive_limit) {
            return bits;
        }
        if (minimum.magnitude <= magnitude_limit && maximum.negative) {
            return bits;
        }
    }
    return std::nullopt;
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

class RecordLayoutAnalyzer {
  public:
    RecordLayoutAnalyzer(lispb::schema::TypeGraph const& types, AbiProfile const& abi)
        : types_{types}
        , abi_{abi} {}

    auto analyze(lispb::schema::TypeId const type) -> RecordAnalysis {
        std::vector<lispb::schema::TypeId> active;
        return analyze(type, active);
    }
  private:
    auto facts_for(lispb::schema::TypeId const type,
                   std::vector<lispb::schema::TypeId>& active,
                   std::vector<Diagnostic>& diagnostics,
                   std::string const& context) -> std::optional<TypeFacts> {
        auto const& node{types_.type(type)};
        if (std::holds_alternative<lispb::schema::RecordType>(node.definition)) {
            auto nested{analyze(type, active)};
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
                             .unsigned_value_bits = std::nullopt};
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

    auto analyze(lispb::schema::TypeId const type, std::vector<lispb::schema::TypeId>& active)
        -> RecordAnalysis {
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

    lispb::schema::TypeGraph const& types_;
    AbiProfile const& abi_;
};

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

auto format_enum_code(EnumCodeValue const value) -> std::string {
    return (value.negative ? "-" : "") + std::to_string(value.magnitude);
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
                              .minimum_required_bits = std::nullopt,
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

    std::optional<EnumCodeValue> previous;
    bool has_prior_enumerator{};
    bool all_codes_known{true};
    std::set<std::pair<bool, std::uint64_t>> distinct_codes;
    result.enumerators.reserve(enumeration.enumerators.size());
    for (auto const& enumerator : enumeration.enumerators) {
        std::optional<EnumCodeValue> code;
        if (enumerator.explicit_value.has_value()) {
            code = parse_enum_code(*enumerator.explicit_value);
            if (!code.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Enumerator '" + enumerator.name + "' uses non-literal initializer '" +
                         *enumerator.explicit_value + "'; value-domain width is unknown."});
            }
        } else if (!has_prior_enumerator) {
            code = EnumCodeValue{.negative = false, .magnitude = 0};
        } else if (previous.has_value()) {
            code = next_enum_code(*previous);
            if (!code.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Implicit value for enumerator '" + enumerator.name +
                         "' overflows the supported 64-bit analysis range."});
            }
        } else {
            result.diagnostics.push_back({DiagnosticSeverity::warning,
                                          "Implicit value for enumerator '" + enumerator.name +
                                              "' cannot be derived after an unknown initializer."});
        }
        has_prior_enumerator = true;

        if (enumerator.count_sentinel) {
            ++result.reserved_value_count;
        } else {
            ++result.live_value_count;
        }
        if (code.has_value()) {
            previous = code;
            distinct_codes.emplace(code->negative, code->magnitude);
            if (!result.minimum_value.has_value() || enum_code_less(*code, *result.minimum_value)) {
                result.minimum_value = code;
            }
            if (!result.maximum_value.has_value() || enum_code_less(*result.maximum_value, *code)) {
                result.maximum_value = code;
            }
        } else {
            previous.reset();
            all_codes_known = false;
        }
        result.enumerators.push_back(
            {.name = enumerator.name, .count_sentinel = enumerator.count_sentinel, .code = code});
    }

    if (all_codes_known && result.minimum_value.has_value() && result.maximum_value.has_value()) {
        result.signed_domain = result.minimum_value->negative;
        result.minimum_required_bits =
            minimum_enum_bits(*result.minimum_value, *result.maximum_value);
        if (!result.minimum_required_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Enum value domain exceeds the supported signed 64-bit analysis range."});
        }
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
                     "Enum value domain " + format_enum_code(*result.minimum_value) + " .. " +
                         format_enum_code(*result.maximum_value) + " does not fit backing type '" +
                         backing_type + "'."});
            }
        }
        if (result.backing_bits.has_value() && *result.backing_bits < 64) {
            auto const backing_codes{std::uint64_t{1} << *result.backing_bits};
            if (distinct_codes.size() <= backing_codes) {
                result.unused_backing_codes = backing_codes - distinct_codes.size();
            }
        }
    }
    return result;
}

auto Analyzer::analyze_record(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              AbiProfile const& abi,
                              std::uint64_t const element_count) -> RecordAnalysis {
    auto result{RecordLayoutAnalyzer{types, abi}.analyze(type)};
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
                          .unused_bits = std::nullopt,
                          .overflow_bits = std::nullopt,
                          .invalid_raw_value = packed.invalid_raw_value,
                          .fields = {},
                          .diagnostics = {},
                          .aggregate = {.element_count = element_count,
                                        .total_storage_bytes = std::nullopt,
                                        .total_payload_bits = std::nullopt,
                                        .total_unused_bits = std::nullopt,
                                        .cache_line_bytes = std::nullopt,
                                        .minimum_cache_lines = std::nullopt,
                                        .complete_elements_per_cache_line = std::nullopt,
                                        .page_bytes = std::nullopt,
                                        .minimum_pages = std::nullopt,
                                        .complete_elements_per_page = std::nullopt}};
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
    bool width_overflow{};
    result.fields.reserve(packed.fields.size());
    for (auto const& field : packed.fields) {
        auto const width{effective_field_width(type, field, variant)};
        auto const next_offset{checked_add(offset, width)};
        PackedFieldAnalysis field_result{
            .name = field.name,
            .semantic_type = field.semantic_type.type,
            .logical_type = types.type(field.semantic_type.type).cpp_spelling,
            .kind = field.kind,
            .schema_bit_width = field.bit_width,
            .bit_width = width,
            .overridden = variant.overrides.packed_field_widths.contains(
                FieldOverrideId{.type = type, .field_name = field.name}),
            .least_significant_bit = offset,
            .most_significant_bit = std::nullopt,
            .maximum_unsigned_value = maximum_unsigned_value(width)};
        if (width == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field.name + "' must use at least one bit."});
        }
        if (!field_result.maximum_unsigned_value.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field.name + "' exceeds the 64-bit V1 range limit."});
        }
        if (next_offset.has_value()) {
            if (width != 0) {
                field_result.most_significant_bit = *next_offset - 1;
            }
            offset = *next_offset;
        } else {
            width_overflow = true;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed field bit offsets overflow uint64."});
        }
        result.fields.push_back(std::move(field_result));
    }

    if (!width_overflow) {
        result.bits_used = offset;
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
    if (result.bits_used.has_value()) {
        result.aggregate.total_payload_bits = checked_multiply(*result.bits_used, element_count);
        if (!result.aggregate.total_payload_bits.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Packed aggregate payload bit count overflows uint64."});
        }
    }
    if (result.unused_bits.has_value()) {
        result.aggregate.total_unused_bits = checked_multiply(*result.unused_bits, element_count);
        if (!result.aggregate.total_unused_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed aggregate unused bit count overflows uint64."});
        }
    }
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
    if (pages_complete) {
        result.minimum_pages = total_pages;
    }
    return result;
}

} // namespace ioj::layout

#include <lispb/schema/enum_domain.h>

#include <codegen/schema/enum_schema.h>

#include <cctype>
#include <charconv>
#include <limits>
#include <set>
#include <string_view>

namespace lispb::schema {
namespace {

auto parse_enum_code(std::string_view text) -> std::optional<EnumCode> {
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
    return EnumCode{.negative = negative && magnitude != 0, .magnitude = magnitude};
}

auto next_enum_code(EnumCode const value) -> std::optional<EnumCode> {
    if (value.negative) {
        return value.magnitude == 1
                 ? std::optional{EnumCode{.negative = false, .magnitude = 0}}
                 : std::optional{EnumCode{.negative = true, .magnitude = value.magnitude - 1}};
    }
    if (value.magnitude == (std::numeric_limits<std::uint64_t>::max)()) {
        return std::nullopt;
    }
    return EnumCode{.negative = false, .magnitude = value.magnitude + 1};
}

auto enum_code_less(EnumCode const left, EnumCode const right) -> bool {
    if (left.negative != right.negative) {
        return left.negative;
    }
    return left.negative ? left.magnitude > right.magnitude : left.magnitude < right.magnitude;
}

auto minimum_enum_bits(EnumCode const minimum, EnumCode const maximum, bool const signed_domain)
    -> std::optional<std::uint32_t> {
    if (!signed_domain) {
        if (minimum.negative) {
            return std::nullopt;
        }
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
        if (minimum.magnitude <= magnitude_limit &&
            (maximum.negative || maximum.magnitude <= positive_limit)) {
            return bits;
        }
    }
    return std::nullopt;
}

} // namespace

auto analyze_enum_domain(std::span<EnumDomainInput const> const values,
                         std::optional<bool> const signedness) -> EnumDomain {
    EnumDomain result;
    std::optional<EnumCode> previous;
    bool has_prior_value{};
    bool all_codes_known{true};
    std::set<std::pair<bool, std::uint64_t>> distinct_codes;
    result.values.reserve(values.size());

    for (auto const& value : values) {
        std::optional<EnumCode> code;
        if (value.explicit_value.has_value()) {
            code = parse_enum_code(*value.explicit_value);
            if (!code.has_value()) {
                result.issues.push_back(
                    {.severity = EnumDomainIssueSeverity::warning,
                     .message = "Enumerator '" + value.name + "' uses non-literal initializer '" +
                              *value.explicit_value + "'; value-domain width is unknown."});
            }
        } else if (!has_prior_value) {
            code = EnumCode{.negative = false, .magnitude = 0};
        } else if (previous.has_value()) {
            code = next_enum_code(*previous);
            if (!code.has_value()) {
                result.issues.push_back(
                    {.severity = EnumDomainIssueSeverity::error,
                     .message = "Implicit value for enumerator '" + value.name +
                                "' overflows the supported 64-bit analysis range."});
            }
        } else {
            result.issues.push_back(
                {.severity = EnumDomainIssueSeverity::warning,
                 .message = "Implicit value for enumerator '" + value.name +
                            "' cannot be derived after an unknown initializer."});
        }
        has_prior_value = true;

        if (value.reserved) {
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
        result.values.push_back(
            {.name = value.name, .reserved = value.reserved, .code = std::move(code)});
    }

    result.distinct_code_count = distinct_codes.size();
    result.signed_domain = signedness;
    if (all_codes_known && result.minimum_value.has_value() && result.maximum_value.has_value()) {
        auto const effective_signedness{signedness.value_or(result.minimum_value->negative)};
        result.signed_domain = effective_signedness;
        if (!effective_signedness && result.minimum_value->negative) {
            result.issues.push_back(
                {.severity = EnumDomainIssueSeverity::error,
                 .message = "Explicitly unsigned enum domain contains negative value " +
                            format_enum_code(*result.minimum_value) + "."});
        } else {
            result.minimum_required_bits = minimum_enum_bits(
                *result.minimum_value, *result.maximum_value, effective_signedness);
            if (!result.minimum_required_bits.has_value()) {
                result.issues.push_back(
                    {.severity = EnumDomainIssueSeverity::error,
                     .message = "Enum value domain does not fit its signedness within the "
                                "supported 64-bit analysis range."});
            }
        }
    }
    return result;
}

auto analyze_enum_domain(codegen::EnumSchema const& schema) -> EnumDomain {
    std::vector<EnumDomainInput> values;
    values.reserve(schema.values.size());
    for (auto const& value : schema.values) {
        values.push_back({.name = value.name,
                          .explicit_value = value.initializer,
                          .reserved = value.sentinel ||
                                      (schema.count.has_value() && value.name == *schema.count)});
    }
    return analyze_enum_domain(values, schema.signedness);
}

auto derive_enum_storage_requirement(EnumDomain const& domain,
                                     std::optional<std::uint32_t> const declared_bit_width)
    -> std::optional<EnumStorageRequirement> {
    auto const effective_width{declared_bit_width.has_value() ? declared_bit_width
                                                              : domain.minimum_required_bits};
    if (!domain.signed_domain.has_value() || !effective_width.has_value() ||
        *effective_width == 0 || *effective_width > 64) {
        return std::nullopt;
    }

    auto const storage_width{*effective_width <= 8    ? 8U
                             : *effective_width <= 16 ? 16U
                             : *effective_width <= 32 ? 32U
                                                      : 64U};
    return EnumStorageRequirement{.signedness = *domain.signed_domain, .bit_width = storage_width};
}

auto format_enum_code(EnumCode const value) -> std::string {
    return (value.negative ? "-" : "") + std::to_string(value.magnitude);
}

} // namespace lispb::schema

#include "integer_code_domain.hpp"

#include "analysis_limits.hpp"
#include "checked_arithmetic.hpp"

#include <codegen/schema/packed_integer_value.h>

#include <cmath>
#include <cstdint>
#include <optional>

namespace ioj::layout::analysis_detail {

auto maximum_unsigned_value(std::uint32_t const bits) -> std::optional<std::uint64_t> {
    if (bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return maximum_unsigned_code;
    }
    return (std::uint64_t{1} << bits) - 1;
}

auto minimum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return minimum_signed_code;
    }
    return -static_cast<std::int64_t>(std::uint64_t{1} << (bits - 1));
}

auto maximum_signed_value(std::uint32_t const bits) -> std::optional<std::int64_t> {
    if (bits == 0 || bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return maximum_signed_code;
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

auto packed_range_count(lispb::codegen::PackedIntegerValue const minimum,
                        lispb::codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t> {
    if (lispb::codegen::packed_integer_less(maximum, minimum)) {
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

auto packed_range_span(lispb::codegen::PackedIntegerValue const minimum,
                       lispb::codegen::PackedIntegerValue const maximum)
    -> std::optional<std::uint64_t> {
    if (lispb::codegen::packed_integer_less(maximum, minimum)) {
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

auto packed_integer_as_long_double(lispb::codegen::PackedIntegerValue const value) -> long double {
    auto const magnitude{static_cast<long double>(value.magnitude)};
    return value.negative ? -magnitude : magnitude;
}

auto exact_code_count_delta(ExactCodeCount const baseline, ExactCodeCount const variant)
    -> NumericDelta {
    if (baseline == variant) {
        return {.direction = NumericDeltaDirection::unchanged, .magnitude = 0, .percentage = 0.0};
    }

    auto const increased{variant.two_to_64 ||
                         (!baseline.two_to_64 && variant.value > baseline.value)};
    auto const& smaller{increased ? baseline : variant};
    auto const& larger{increased ? variant : baseline};
    auto const magnitude{larger.two_to_64 ? maximum_analysis_count - smaller.value + 1
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

}

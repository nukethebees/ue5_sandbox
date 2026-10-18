#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/analyzer/exact_code_count.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct FloatEncoding {
    std::uint32_t sign_bits{};
    std::uint32_t exponent_bits{};
    std::uint32_t significand_bits{};
    std::uint32_t total_bits{};
    std::int32_t exponent_bias{};
    std::uint64_t exponent_code_count{};
    std::uint64_t normal_exponent_code_count{};
    std::int32_t minimum_normal_exponent{};
    std::int32_t maximum_normal_exponent{};
    ExactCodeCount total_code_count{};
    std::uint64_t zero_code_count{};
    std::uint64_t infinity_code_count{};
    std::uint64_t nan_code_count{};
    std::uint64_t nonzero_subnormal_code_count{};
    std::optional<long double> minimum_positive_subnormal{};
    std::optional<long double> minimum_positive_normal{};
    std::optional<long double> maximum_finite{};
    std::optional<long double> minimum_finite{};
    std::optional<long double> unit_interval_resolution{};
    long double maximum_relative_rounding_error{};
};

}

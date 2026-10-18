#pragma once

#include "analysis_limits.hpp"

#include <cstdint>
#include <optional>

namespace ioj::layout::analysis_detail {

inline auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (right > maximum_analysis_count - left) {
        return std::nullopt;
    }
    return left + right;
}

inline auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (left != 0 && right > maximum_analysis_count / left) {
        return std::nullopt;
    }
    return left * right;
}

inline auto align_up(std::uint64_t const value, std::uint64_t const alignment)
    -> std::optional<std::uint64_t> {
    if (alignment == 0) {
        return std::nullopt;
    }
    auto const remainder{value % alignment};
    return remainder == 0 ? std::optional{value} : checked_add(value, alignment - remainder);
}

}

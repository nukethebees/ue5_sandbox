#pragma once

#include <cstdint>
#include <optional>

namespace ioj::layout::analysis_detail {

auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t>;

auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t>;

auto align_up(std::uint64_t const value, std::uint64_t const alignment)
    -> std::optional<std::uint64_t>;

}

#pragma once

#include <algorithm>
#include <string_view>

namespace ioj {
[[nodiscard]] constexpr auto is_ascii_lower(char const value) noexcept -> bool {
    return value >= 'a' && value <= 'z';
}
[[nodiscard]] constexpr auto is_ascii_digit(char const value) noexcept -> bool {
    return value >= '0' && value <= '9';
}
[[nodiscard]] constexpr auto to_ascii_lower(char const value) noexcept -> char {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}
[[nodiscard]] constexpr auto is_ascii_space(char const value) noexcept -> bool {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' ||
           value == '\v';
}
[[nodiscard]] constexpr auto blank(std::string_view const value) noexcept -> bool {
    return std::ranges::all_of(value, is_ascii_space);
}
}

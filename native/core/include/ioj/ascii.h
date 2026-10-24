#pragma once

#include <algorithm>
#include <string_view>

namespace ioj {
// Classify ASCII characters; preserve other bytes during case conversion.
[[nodiscard]] constexpr auto islower(char const value) noexcept -> bool {
    return value >= 'a' && value <= 'z';
}
[[nodiscard]] constexpr auto isupper(char const value) noexcept -> bool {
    return value >= 'A' && value <= 'Z';
}
[[nodiscard]] constexpr auto isdigit(char const value) noexcept -> bool {
    return value >= '0' && value <= '9';
}
[[nodiscard]] constexpr auto isprint(char const value) noexcept -> bool {
    return value >= ' ' && value <= '~';
}
[[nodiscard]] constexpr auto needs_char_literal_escape(char const value) noexcept -> bool {
    return value == '\'' || value == '\\';
}
[[nodiscard]] constexpr auto tolower(char const value) noexcept -> char {
    return isupper(value) ? static_cast<char>(value + ('a' - 'A')) : value;
}
[[nodiscard]] constexpr auto toupper(char const value) noexcept -> char {
    return islower(value) ? static_cast<char>(value - ('a' - 'A')) : value;
}
[[nodiscard]] constexpr auto isspace(char const value) noexcept -> bool {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' ||
           value == '\v';
}
[[nodiscard]] constexpr auto blank(std::string_view const value) noexcept -> bool {
    return std::ranges::all_of(value, ioj::isspace);
}
[[nodiscard]] constexpr auto equals_ignore_case(std::string_view const lhs,
                                                std::string_view const rhs) noexcept -> bool {
    return std::ranges::equal(lhs, rhs, std::ranges::equal_to{}, ioj::tolower, ioj::tolower);
}
}

#pragma once

#include <string>
#include <string_view>

namespace ioj::s7 {
namespace literals {
inline constexpr std::string_view true_value{"#t"};
inline constexpr std::string_view false_value{"#f"};
inline constexpr std::string_view nan{"+nan.0"};
inline constexpr std::string_view positive_infinity{"+inf.0"};
inline constexpr std::string_view negative_infinity{"-inf.0"};
}

// Preserve the real type when the emitted value is read back by Scheme.
[[nodiscard]] auto format_real(double value) -> std::string;
}

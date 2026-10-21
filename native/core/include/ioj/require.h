#pragma once

#include <source_location>
#include <string_view>

namespace ioj {
namespace detail {
[[noreturn]] void require_failed(std::string_view message, std::source_location location) noexcept;
}

inline void
    require(bool const condition,
            std::string_view const message,
            std::source_location const location = std::source_location::current()) noexcept {
    if (!condition) [[unlikely]] {
        detail::require_failed(message, location);
    }
}
} // namespace ioj

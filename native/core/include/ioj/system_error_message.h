#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace ioj {
inline auto system_error_message(std::string_view operation, std::uint32_t code) -> std::string {
    return std::format(
        "{} ({}): {}", operation, code, std::system_category().message(static_cast<int>(code)));
}
}

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ioj::s7::detail {
[[nodiscard]] auto utf8_to_wide(std::string_view value) -> std::optional<std::wstring>;
[[nodiscard]] auto wide_to_utf8(std::wstring_view value) -> std::optional<std::string>;
}

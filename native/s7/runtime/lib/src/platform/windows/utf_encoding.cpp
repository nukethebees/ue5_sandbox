#include "utf_encoding.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ioj::s7::detail {
[[nodiscard]] auto utf8_to_wide(std::string_view const value) -> std::optional<std::wstring> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(output_size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), input_size, result.data(), output_size) !=
        output_size) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto wide_to_utf8(std::wstring_view const value) -> std::optional<std::string> {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    auto const input_size{static_cast<int>(value.size())};
    auto const output_size{WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), input_size, nullptr, 0, nullptr, nullptr)};
    if (output_size <= 0) {
        return std::nullopt;
    }

    std::string result(static_cast<std::size_t>(output_size), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            value.data(),
                            input_size,
                            result.data(),
                            output_size,
                            nullptr,
                            nullptr) != output_size) {
        return std::nullopt;
    }
    return result;
}

}

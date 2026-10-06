#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace ml::s7::detail {
struct CanonicalPath {
    std::wstring wide_path;
    std::string narrow_path;
    std::size_t file_size{};
};

[[nodiscard]] auto path_key(std::string key) -> std::string;
[[nodiscard]] auto resolve_script_file(std::string_view root_utf8, std::string_view requested_path)
    -> std::expected<CanonicalPath, std::string>;
}

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lispb::codegen {

inline auto generic_path_utf8(std::filesystem::path const& path) -> std::string {
    auto const bytes{path.generic_u8string()};
    return {bytes.begin(), bytes.end()};
}

inline auto path_from_utf8(std::string_view bytes) -> std::filesystem::path {
    return std::filesystem::path{std::u8string{bytes.begin(), bytes.end()}};
}

auto output_path_key(std::filesystem::path const& path) -> std::string;

}

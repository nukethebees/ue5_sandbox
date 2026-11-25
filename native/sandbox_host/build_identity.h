#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace ioj::sandbox_host {
auto read_build_identity(std::filesystem::path const& path)
    -> std::expected<std::string, std::string>;
auto verify_build_identity(std::filesystem::path const& path, std::string_view expected)
    -> std::expected<void, std::string>;
}

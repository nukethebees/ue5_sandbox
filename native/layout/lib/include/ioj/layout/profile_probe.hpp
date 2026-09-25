#pragma once

#include <expected>
#include <span>
#include <string>

namespace ioj::layout {

// Produces standalone C++17 source. Compile in the target SDK; stdout is an importable profile.
auto profile_probe_source(std::span<std::string const> types, std::span<std::string const> headers)
    -> std::expected<std::string, std::string>;

} // namespace ioj::layout

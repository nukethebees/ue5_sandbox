#pragma once

#include <filesystem>
#include <string>

namespace lispb::codegen::detail {

auto format_generated(std::string const& content, std::filesystem::path const& destination)
    -> std::string;

}

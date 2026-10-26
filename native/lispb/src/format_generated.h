#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {

struct FormatInput {
    std::string_view content;
    std::filesystem::path destination;
};

auto format_generated(std::span<FormatInput const> files) -> std::vector<std::string>;

}

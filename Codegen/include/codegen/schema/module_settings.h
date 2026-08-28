#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct ModuleSettings {
    std::string name;
    std::filesystem::path header;
    std::optional<std::filesystem::path> source;
    std::optional<std::string> header_include;
    std::optional<std::string> namespace_name;
    std::vector<std::string> include_order;
    std::vector<std::string> prelude_lines;
};

} // namespace codegen

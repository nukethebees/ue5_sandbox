#pragma once

#include <filesystem>
#include <string>

namespace codegen {

struct GeneratedFile {
    std::filesystem::path path;
    std::string content;
    bool format_generated{false};

    auto operator==(GeneratedFile const&) const -> bool = default;
};

} // namespace codegen

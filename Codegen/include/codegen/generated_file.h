#pragma once

#include <filesystem>
#include <string>

namespace codegen {

struct GeneratedFile {
    std::filesystem::path path;
    std::string content;

    auto operator==(GeneratedFile const&) const -> bool = default;
};

} // namespace codegen

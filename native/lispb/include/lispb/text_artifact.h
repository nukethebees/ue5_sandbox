#pragma once

#include <filesystem>
#include <string>
namespace lispb {

struct TextArtifact {
    std::filesystem::path path;
    std::string content;
    bool format_generated{false};

    auto operator==(TextArtifact const&) const -> bool = default;
};

}

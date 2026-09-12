#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lispb {

struct TextArtifact {
    std::filesystem::path path;
    std::string content;
    bool format_generated{false};

    auto operator==(TextArtifact const&) const -> bool = default;
};

struct BinaryArtifact {
    std::filesystem::path path;
    std::vector<std::uint8_t> content;

    auto operator==(BinaryArtifact const&) const -> bool = default;
};

using Artifact = std::variant<TextArtifact, BinaryArtifact>;

struct Compilation {
    std::vector<Artifact> artifacts;
    std::vector<std::filesystem::path> dependencies;
    std::optional<std::string> debug_dump;
};

} // namespace lispb

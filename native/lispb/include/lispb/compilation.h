#pragma once

#include <lispb/artifact_id.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace lispb {

struct Compilation {
    std::vector<ArtifactId> artifacts;
    std::vector<std::filesystem::path> dependencies;
    std::optional<std::string> debug_dump;

    void add_text(std::filesystem::path path, std::string content, bool format_generated = false);

    void add_binary(std::filesystem::path path, std::vector<std::uint8_t> content);

    auto path(ArtifactId const id) const -> std::filesystem::path const&;

    auto text(ArtifactId const id) const -> std::string const&;

    auto binary(ArtifactId const id) const -> std::span<std::uint8_t const>;

    auto needs_formatting(ArtifactId const id) const -> bool;

    void set_formatted_text(ArtifactId const id, std::string content);
  private:
    std::vector<std::filesystem::path> text_paths_;
    std::vector<std::string> text_contents_;
    std::vector<bool> text_formatting_;
    std::vector<std::filesystem::path> binary_paths_;
    std::vector<std::vector<std::uint8_t>> binary_contents_;
};

}

#pragma once
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/levels/authoring/level_parser.h>

#include <filesystem>
#include <string_view>

namespace ioj::levels::authoring {
// Evaluate a root and its explicit imports in one interpreter, release it, then parse and validate.
class DefinitionReader {
  public:
    DefinitionReader() = default;
    explicit DefinitionReader(std::filesystem::path script_library_root);
    // Resolve all imports relative to the root file's directory. Each call starts fresh.
    [[nodiscard]] auto read_root_file(std::filesystem::path const& path) const
        -> std::expected<DefinitionCatalog, Diagnostics>;
    [[nodiscard]] auto read_level_source(std::string_view source) const
        -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_source(std::string_view source) const
        -> CampaignDefinitionReadResult;
    // Standalone reads use this reader's import root, or the conventional level/campaign directory.
    [[nodiscard]] auto read_level_file(std::filesystem::path const& path) const
        -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_file(std::filesystem::path const& path) const
        -> CampaignDefinitionReadResult;
  private:
    std::filesystem::path script_library_root_{};
};
}

#pragma once
#include <ioj/levels/authoring/campaign_parser.h>
#include <ioj/levels/authoring/level_parser.h>

#include <filesystem>
#include <string_view>

namespace ioj::levels::authoring {
// Evaluate each definition in an isolated interpreter, release it, then parse and validate.
// Library scripts are loaded explicitly by the definition, never evaluated as catalog entries.
class DefinitionReader {
  public:
    DefinitionReader() = default;
    explicit DefinitionReader(std::filesystem::path script_library_root);
    [[nodiscard]] auto read_level_source(std::string_view source) const
        -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_source(std::string_view source) const
        -> CampaignDefinitionReadResult;
    // Default library locations match the catalog: levels beside Libraries, campaigns in Campaigns.
    [[nodiscard]] auto read_level_file(std::filesystem::path const& path) const
        -> LevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_file(std::filesystem::path const& path) const
        -> CampaignDefinitionReadResult;
  private:
    std::filesystem::path script_library_root_{};
};
}

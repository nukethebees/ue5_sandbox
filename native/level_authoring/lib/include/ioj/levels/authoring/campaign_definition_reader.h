#pragma once
#include <ioj/levels/campaign_definition.h>
#include <ioj/s7/ast.h>

#include <filesystem>
#include <string_view>

namespace ioj::levels::authoring {
using CampaignDefinitionReadResult = std::expected<CampaignDefinition, Diagnostics>;
[[nodiscard]] auto parse_campaign(s7::Ast const& ast) -> CampaignDefinitionReadResult;
class CampaignDefinitionReader {
  public:
    CampaignDefinitionReader() = default;
    explicit CampaignDefinitionReader(std::filesystem::path script_library_root);
    [[nodiscard]] auto read_source(std::string_view source) const -> CampaignDefinitionReadResult;
    [[nodiscard]] auto read_file(std::filesystem::path const& path) const
        -> CampaignDefinitionReadResult;
  private:
    std::filesystem::path script_library_root_{};
};
}

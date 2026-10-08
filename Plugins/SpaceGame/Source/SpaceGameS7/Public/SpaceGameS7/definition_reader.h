#pragma once
#include <ioj/levels/diagnostics.h>
#include <SpaceGame/levels/CampaignDefinition.h>
#include <SpaceGame/levels/LevelDefinition.h>

#include <expected>
namespace ioj::levels::authoring {
using FLevelDefinitionReadResult = std::expected<ml::FLevelDefinition, Diagnostics>;
using FCampaignDefinitionReadResult = std::expected<ml::FCampaignDefinition, Diagnostics>;
class SPACEGAMES7_API FDefinitionReader {
  public:
    FDefinitionReader() = default;
    explicit FDefinitionReader(FString script_library_root);
    [[nodiscard]] auto read_level_source(FStringView source) const -> FLevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_source(FStringView source) const
        -> FCampaignDefinitionReadResult;
    [[nodiscard]] auto read_level_file(FStringView path) const -> FLevelDefinitionReadResult;
    [[nodiscard]] auto read_campaign_file(FStringView path) const -> FCampaignDefinitionReadResult;
  private:
    FString script_library_root_{};
};
}

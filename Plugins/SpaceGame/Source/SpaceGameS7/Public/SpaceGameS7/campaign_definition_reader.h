#pragma once
#include <ioj/levels/diagnostics.h>
#include <SpaceGame/levels/CampaignDefinition.h>

#include <expected>

namespace ioj::levels::authoring {
using FCampaignDefinitionReadResult = std::expected<ml::FCampaignDefinition, Diagnostics>;
class SPACEGAMES7_API FCampaignDefinitionReader {
  public:
    FCampaignDefinitionReader() = default;
    explicit FCampaignDefinitionReader(FString script_library_root);
    [[nodiscard]] auto read_source(FStringView source) const -> FCampaignDefinitionReadResult;
    [[nodiscard]] auto read_file(FStringView path) const -> FCampaignDefinitionReadResult;
  private:
    FString script_library_root_{};
};
}

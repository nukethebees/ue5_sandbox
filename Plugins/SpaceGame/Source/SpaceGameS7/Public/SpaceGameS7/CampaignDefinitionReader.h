#pragma once

#include <SpaceGame/levels/CampaignDefinition.h>

namespace ml::s7 {
struct SPACEGAMES7_API FCampaignDefinitionDecodeError {
    FString path{};
    FString message{};
};

struct SPACEGAMES7_API FCampaignDefinitionReadResult {
    TOptional<FCampaignDefinition> definition{};
    FString script_error{};
    TArray<FCampaignDefinitionDecodeError> decode_errors{};

    explicit operator bool() const noexcept { return definition.IsSet(); }
};

class SPACEGAMES7_API FCampaignDefinitionReader final {
  public:
    FCampaignDefinitionReader() = default;
    explicit FCampaignDefinitionReader(FString script_library_root);

    [[nodiscard]] auto read_source(FStringView source) const -> FCampaignDefinitionReadResult;
    [[nodiscard]] auto read_file(FStringView path) const -> FCampaignDefinitionReadResult;
  private:
    FString script_library_root_{};
};
}

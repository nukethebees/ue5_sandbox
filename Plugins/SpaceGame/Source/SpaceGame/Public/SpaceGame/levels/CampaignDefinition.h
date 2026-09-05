#pragma once

#include <SpaceGame/levels/LevelTypes.h>

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FCampaignDefinition {
    FCampaignDefinition();
    FCampaignDefinition(FCampaignDefinition const&);
    FCampaignDefinition(FCampaignDefinition&&) noexcept;
    auto operator=(FCampaignDefinition const&) -> FCampaignDefinition&;
    auto operator=(FCampaignDefinition&&) noexcept -> FCampaignDefinition&;
    ~FCampaignDefinition();

    FCampaignId id{};
    FString title{};
    TArray<FLevelId> level_ids{};
};
}

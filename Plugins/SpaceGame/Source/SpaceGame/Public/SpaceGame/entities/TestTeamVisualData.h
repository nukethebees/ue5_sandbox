#pragma once

#include <SpaceGame/entities/TeamColours.h>

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Math/Color.h>

#include <utility>

#include "TestTeamVisualData.generated.h"

UCLASS(BlueprintType)
class UTestTeamVisualData : public UDataAsset {
    GENERATED_BODY()
  public:
    using FColourArray = FTeamColours;

    static constexpr FLinearColor missing_colour{1.f, 0.f, 1.f, 1.f};

    UTestTeamVisualData();

    void PostLoad() override;

    auto get_colour(ETestTeam team) const -> FLinearColor {
        return colours.FindRef(team, missing_colour);
    }

    auto build_team_colour_cache() const -> FColourArray;
    static auto build_team_colour_cache(UTestTeamVisualData const* const data) -> FColourArray;
    void ensure_all_team_colours_exist();

    UPROPERTY(EditDefaultsOnly)
    TMap<ETestTeam, FLinearColor> colours;
};

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

#include "TeamID.generated.h"

UENUM(BlueprintType)
enum class ETeamID : uint8 {
    Player = 0 UMETA(DisplayName = "Player"),
    Friendly = 1 UMETA(DisplayName = "Friendly"),
    Enemy = 2 UMETA(DisplayName = "Enemy"),
    Neutral = 3 UMETA(DisplayName = "Neutral")
};

inline ETeamAttitude::Type GetTeamAttitude(FGenericTeamId TeamA, FGenericTeamId TeamB) {
    auto const team_a{static_cast<ETeamID>(TeamA.GetId())};
    auto const team_b{static_cast<ETeamID>(TeamB.GetId())};

    // Same team is friendly
    if (team_a == team_b) {
        return ETeamAttitude::Friendly;
    }

    // Neutral is neutral to everyone
    if (team_a == ETeamID::Neutral || team_b == ETeamID::Neutral) {
        return ETeamAttitude::Neutral;
    }

    // Player and Friendly are allies
    if ((team_a == ETeamID::Player || team_a == ETeamID::Friendly) &&
        (team_b == ETeamID::Player || team_b == ETeamID::Friendly)) {
        return ETeamAttitude::Friendly;
    }

    // Player/Friendly vs Enemy is hostile
    if ((team_a == ETeamID::Player || team_a == ETeamID::Friendly) && team_b == ETeamID::Enemy) {
        return ETeamAttitude::Hostile;
    }

    // Enemy vs Player/Friendly is hostile
    if (team_a == ETeamID::Enemy && (team_b == ETeamID::Player || team_b == ETeamID::Friendly)) {
        return ETeamAttitude::Hostile;
    }

    // Default to hostile
    return ETeamAttitude::Hostile;
}

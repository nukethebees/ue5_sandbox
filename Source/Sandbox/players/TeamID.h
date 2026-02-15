#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "TeamID.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ETeamID : uint8 {
    Player = 0,
    Friendly = 1,
    Enemy = 2,
    Neutral = 3,
    Count = 4 UMETA(Hidden)
};

inline auto get_team_attitude(ETeamID team_a, ETeamID team_b) -> ETeamAttitude::Type {
    // Same team is friendly
    if (team_a == team_b) {
        return ETeamAttitude::Friendly;
    }

    // Neutral is neutral to everyone
    if (team_a == ETeamID::Neutral || team_b == ETeamID::Neutral) {
        return ETeamAttitude::Neutral;
    }

    auto const non_hostile_a{team_a == ETeamID::Player || team_a == ETeamID::Friendly};
    auto const non_hostile_b{team_b == ETeamID::Player || team_b == ETeamID::Friendly};

    // Player and Friendly are allies
    if (non_hostile_a && non_hostile_b) {
        return ETeamAttitude::Friendly;
    }

    // Player/Friendly vs Enemy is hostile
    if (non_hostile_a && team_b == ETeamID::Enemy) {
        return ETeamAttitude::Hostile;
    }

    // Enemy vs Player/Friendly is hostile
    if (team_a == ETeamID::Enemy && non_hostile_b) {
        return ETeamAttitude::Hostile;
    }

    // Default to hostile
    return ETeamAttitude::Hostile;
}
auto get_team_attitude(ETeamID team_a, AActor& target) -> ETeamAttitude::Type;

inline auto GetTeamAttitude(FGenericTeamId TeamA, FGenericTeamId TeamB) -> ETeamAttitude::Type {
#if !UE_BUILD_SHIPPING
    check(TeamA != FGenericTeamId::NoTeam);
    check(TeamB != FGenericTeamId::NoTeam);
#endif

    auto const team_a{static_cast<ETeamID>(TeamA.GetId())};
    auto const team_b{static_cast<ETeamID>(TeamB.GetId())};

    return get_team_attitude(team_a, team_b);
}

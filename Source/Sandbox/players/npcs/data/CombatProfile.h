#pragma once

#include "CoreMinimal.h"

#include "Sandbox/players/npcs/enums/MobAttackMode.h"

#include "CombatProfile.generated.h"

USTRUCT(BlueprintType)
struct FCombatProfile {
    GENERATED_BODY()

    FCombatProfile() = default;
    FCombatProfile(EMobAttackMode attack_mode, float melee_range, float ranged_range)
        : attack_mode(attack_mode)
        , melee_range(melee_range)
        , ranged_range(ranged_range) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    EMobAttackMode attack_mode{EMobAttackMode::None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float melee_range{150.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float ranged_range{800.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float melee_damage{20.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float melee_cooldown{1.0f};

    float get_attack_range() const {
        switch (attack_mode) {
            case EMobAttackMode::None:
                return melee_range;
            case EMobAttackMode::Melee:
                return melee_range;
            case EMobAttackMode::Ranged:
                return ranged_range;
            default:
                return 0.f;
        }
    }
};

#pragma once

#include "CoreMinimal.h"

#include "Sandbox/enums/MobAttackMode.h"

#include "CombatProfile.generated.h"

USTRUCT(BlueprintType)
struct FCombatProfile {
    GENERATED_BODY()

    FCombatProfile() = default;
    FCombatProfile(EMobAttackMode attack_mode, float melee_range, float ranged_range)
        : attack_mode(attack_mode)
        , melee_range(melee_range)
        , ranged_range(ranged_range) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMobAttackMode attack_mode{EMobAttackMode::None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float melee_range{150.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ranged_range{800.f};

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

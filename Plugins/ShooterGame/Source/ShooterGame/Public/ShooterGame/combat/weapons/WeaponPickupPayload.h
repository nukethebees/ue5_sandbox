#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"

#include "ShooterGame/interaction/TriggerContext.h"
#include "ShooterGame/interaction/TriggerResult.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "WeaponPickupPayload.generated.h"

class AWeaponBase;

USTRUCT(BlueprintType)
struct FWeaponPickupPayload {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    AWeaponBase* weapon{nullptr};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);

    static constexpr auto logger{ml::LogMsgMixin<"FWeaponPickupPayload", LogShooterGameWeapon>{}};
};

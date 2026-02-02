#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"

#include "Sandbox/interaction/TriggerContext.h"
#include "Sandbox/interaction/TriggerResult.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "WeaponPickupPayload.generated.h"

class AWeaponBase;

USTRUCT(BlueprintType)
struct FWeaponPickupPayload {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    AWeaponBase* weapon{nullptr};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);

    static constexpr auto logger{ml::LogMsgMixin<"FWeaponPickupPayload", LogSandboxWeapon>{}};
};

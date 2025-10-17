#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"

#include "Sandbox/data/trigger/TriggerContext.h"
#include "Sandbox/data/trigger/TriggerResult.h"

#include "WeaponPickupPayload.generated.h"

class AWeaponBase;

USTRUCT(BlueprintType)
struct FWeaponPickupPayload {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    AWeaponBase* weapon{nullptr};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);
};

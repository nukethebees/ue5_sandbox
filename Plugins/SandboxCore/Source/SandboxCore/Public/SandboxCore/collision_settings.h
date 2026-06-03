#pragma once

#include <CoreMinimal.h>

#include <Components/PrimitiveComponent.h>
#include <Engine/EngineTypes.h>

#include "collision_settings.generated.h"

class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FCollisionSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TEnumAsByte<ECollisionEnabled::Type> enabled{ECollisionEnabled::NoCollision};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TEnumAsByte<ECollisionChannel> object_type{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCollisionResponseContainer response{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool can_generate_overlap_events{false};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TEnumAsByte<ECanBeCharacterBase> can_character_step_up_on{ECanBeCharacterBase::ECB_No};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    uint8 notify_rigid_body_collision{0u};
};

namespace ml {
auto SANDBOXCORE_API copy_collision_settings(UPrimitiveComponent const& component)
    -> FCollisionSettings;
void SANDBOXCORE_API apply_collision_settings(UPrimitiveComponent& component,
                                              FCollisionSettings const& settings);
}

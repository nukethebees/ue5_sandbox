#include "ShooterGame/items/SpeedBoostItemComponent.h"

#include "ShooterGame/interaction/CollisionEffectSubsystem.h"
#include "ShooterGame/interaction/CollisionPayloads.h"
#include "ShooterGame/players/SpeedBoostComponent.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostItemComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        try_add_subsystem_payload<UCollisionEffectSubsystem>(*owner,
                                                             FSpeedBoostPayload(speed_boost));
    }
}

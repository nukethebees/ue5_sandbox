#include "Sandbox/items/SpeedBoostItemComponent.h"

#include "Sandbox/interaction/CollisionEffectSubsystem.h"
#include "Sandbox/interaction/CollisionPayloads.h"
#include "Sandbox/players/playable/SpeedBoostComponent.h"

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

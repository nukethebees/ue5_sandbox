#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/data/collision/CollisionPayloads.h"
#include "Sandbox/subsystems/world/CollisionEffectSubsystem.h"

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

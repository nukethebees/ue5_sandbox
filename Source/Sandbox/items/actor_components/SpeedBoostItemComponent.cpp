#include "Sandbox/items/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/interaction/collision/data/CollisionPayloads.h"
#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystem.h"
#include "Sandbox/players/playable/actor_components/SpeedBoostComponent.h"

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

#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/data/CollisionPayloads.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostItemComponent::BeginPlay() {
    Super::BeginPlay();

    try_add_subsystem_payload<UCollisionEffectSubsystem>(GetOwner(),
                                                         FSpeedBoostPayload(speed_boost));
}

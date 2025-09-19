#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/data/CollisionPayloads.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem2.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostItemComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        if (auto* world{GetWorld()}) {
            if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem2>()}) {
                subsystem->add_payload(owner, FSpeedBoostPayload(speed_boost));
            }
        }
    }
}

#include "Sandbox/actor_components/HealthChangeTriggerComponent.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"

UHealthChangeTriggerComponent::UHealthChangeTriggerComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthChangeTriggerComponent::BeginPlay() {
    Super::BeginPlay();
}
void UHealthChangeTriggerComponent::change_health(AActor* OverlappedActor, AActor* OtherActor) {
    auto* owner{GetOwner()};

    if (!OtherActor || OtherActor == owner) {
        return;
    }

    if (auto* health{OtherActor->FindComponentByClass<UHealthComponent>()}) {
        if ((health_change.type == EHealthChangeType::Healing) && health->at_max_health()) {
            return;
        }

        if (auto* manager{GetWorld()->GetSubsystem<UDamageManagerSubsystem>()}) {
            manager->queue_health_change(health, health_change);
        }

        owner->Destroy();
    }
}

#include "Sandbox/actor_components/HealthChangeOnOverlapComponent.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"

UHealthChangeOnOverlapComponent::UHealthChangeOnOverlapComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthChangeOnOverlapComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        owner->OnActorBeginOverlap.AddDynamic(this, &UHealthChangeOnOverlapComponent::on_overlap);
    }
}
void UHealthChangeOnOverlapComponent::on_overlap(AActor* OverlappedActor, AActor* OtherActor) {
    auto* owner{GetOwner()};

    if (!OtherActor || OtherActor == owner) {
        return;
    }

    if (auto* health{OtherActor->FindComponentByClass<UHealthComponent>()}) {
        if ((health_change.type == EHealthChangeType::Healing) && health->at_max_health()) {
            return;
        }

        if (auto* manager{GetWorld()->GetSubsystem<UDamageManagerSubsystem>()}) {
            manager->queue_damage(health, health_change);
        }

        owner->Destroy();
    }

    return;
}

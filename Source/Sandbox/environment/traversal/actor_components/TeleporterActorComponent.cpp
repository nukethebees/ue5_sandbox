#include "Sandbox/environment/traversal/actor_components/TeleporterActorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UTeleporterActorComponent::UTeleporterActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UTeleporterActorComponent::BeginPlay() {
    Super::BeginPlay();

    auto* owner{GetOwner()};
    auto comps{
        owner->GetComponentsByTag(UPrimitiveComponent::StaticClass(), FName("TeleportTrigger"))};
    if (comps.Num() > 0) {
        collision_trigger = Cast<UPrimitiveComponent>(comps[0]);
    }

    comps = owner->GetComponentsByTag(USceneComponent::StaticClass(), FName("TeleportTarget"));
    if (comps.Num() > 0) {
        target_location = Cast<USceneComponent>(comps[0]);
    }

    if (collision_trigger) {
        collision_trigger->OnComponentBeginOverlap.AddDynamic(
            this, &UTeleporterActorComponent::HandleOverlap);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("No collision trigger."));
    }

    if (!target_location) {
        UE_LOG(LogTemp, Warning, TEXT("No target location."));
    }
}
void UTeleporterActorComponent::HandleOverlap(UPrimitiveComponent* OverlappedComp,
                                              AActor* other_actor,
                                              UPrimitiveComponent* OtherComp,
                                              int32 other_body_index,
                                              bool from_sweep,
                                              FHitResult const& sweep_result) {
    if (!target_location || !other_actor || other_actor == GetOwner()) {
        UE_LOG(LogTemp, Warning, TEXT("Teleport failed."));
        return;
    }

    other_actor->SetActorLocation(target_location->GetComponentLocation());
}

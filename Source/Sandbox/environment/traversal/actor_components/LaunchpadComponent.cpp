#include "Sandbox/environment/traversal/actor_components/LaunchpadComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

ULaunchpadComponent::ULaunchpadComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void ULaunchpadComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto const owner{GetOwner()}) {
        collision_box = owner->FindComponentByClass<UBoxComponent>();

        if (collision_box != nullptr) {
            collision_box->OnComponentBeginOverlap.AddDynamic(
                this, &ULaunchpadComponent::on_overlap_begin);
        }
    }
}

void ULaunchpadComponent::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                           AActor* other_actor,
                                           UPrimitiveComponent* other_comp,
                                           int32 other_body_index,
                                           bool b_from_sweep,
                                           FHitResult const& sweep_result) {

    if (auto const character{Cast<ACharacter>(other_actor)}) {
        FVector const launch_velocity{0.0f, 0.0f, launch_strength};
        character->LaunchCharacter(launch_velocity, true, true);
    }
}

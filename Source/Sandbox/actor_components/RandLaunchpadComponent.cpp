#include "Sandbox/actor_components/RandLaunchpadComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"

URandLaunchpadComponent::URandLaunchpadComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void URandLaunchpadComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto const owner{GetOwner()}) {
        collision_box = owner->FindComponentByClass<UBoxComponent>();

        if (collision_box != nullptr) {
            collision_box->OnComponentBeginOverlap.AddDynamic(
                this, &URandLaunchpadComponent::on_overlap_begin);
        }
    }
}

void URandLaunchpadComponent::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                               AActor* other_actor,
                                               UPrimitiveComponent* other_comp,
                                               int32 other_body_index,
                                               bool b_from_sweep,
                                               FHitResult const& sweep_result) {

    if (auto* const character{Cast<ACharacter>(other_actor)}) {
        FVector const launch_velocity{FMath::RandRange(-horizontal_spread, horizontal_spread),
                                      FMath::RandRange(-horizontal_spread, horizontal_spread),
                                      launch_strength};

        character->LaunchCharacter(launch_velocity, true, true);
    }
}

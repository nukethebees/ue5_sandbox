#include "Sandbox/environment/traversal/actor_components/SlidingPlatformActorComponent.h"

USlidingPlatformActorComponent::USlidingPlatformActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void USlidingPlatformActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        start_location = owner->GetActorLocation();
        end_location = start_location + owner->GetActorForwardVector() * move_distance;
    }

    SetComponentTickEnabled(false);
}

void USlidingPlatformActorComponent::TickComponent(float DeltaTime,
                                                   ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (auto* owner{GetOwner()}) {
        auto const current_location{owner->GetActorLocation()};
        auto const target_location{moving_forward ? end_location : start_location};
        auto const direction{(target_location - current_location).GetSafeNormal()};
        auto const step{move_speed * DeltaTime};
        auto const distance_to_target{FVector::Dist(current_location, target_location)};

        if (step >= distance_to_target) {
            owner->SetActorLocation(target_location);
            moving_forward = !moving_forward;
            SetComponentTickEnabled(false);
        } else {
            owner->SetActorLocation(current_location + direction * step);
        }
    }
}
void USlidingPlatformActorComponent::OnClicked() {
    SetComponentTickEnabled(true);
}

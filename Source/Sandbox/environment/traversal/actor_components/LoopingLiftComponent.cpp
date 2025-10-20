#include "Sandbox/environment/traversal/actor_components/LoopingLiftComponent.h"

ULoopingLiftComponent::ULoopingLiftComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void ULoopingLiftComponent::BeginPlay() {
    Super::BeginPlay();
    origin = GetOwner()->GetActorLocation();
    direction.Normalize();
    current_direction = direction;
    original_direction = direction;
}

void ULoopingLiftComponent::TickComponent(float DeltaTime,
                                          ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (pause_timer <= pause_after_completion) {
        pause_timer += DeltaTime;
        return;
    }

    auto const current_location{GetOwner()->GetActorLocation()};
    auto const dist_from_origin{current_location - origin};
    auto const projected_distance{FVector::DotProduct(dist_from_origin, original_direction)};

    bool const going_up{current_direction == original_direction};
    bool const destination_not_reached{going_up ? projected_distance < distance
                                                : projected_distance > 0.0f};

    if (destination_not_reached) {
        GetOwner()->AddActorWorldOffset(current_direction * speed * DeltaTime);
    } else {
        pause_timer = 0.0f;
        current_direction *= -1.0f;
        auto x{1};
    }
}

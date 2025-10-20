#include "Sandbox/environment/traversal/actor_components/LoopingPlatformComponent.h"

#include "Components/SceneComponent.h"

ULoopingPlatformComponent::ULoopingPlatformComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void ULoopingPlatformComponent::BeginPlay() {
    Super::BeginPlay();

    location_a = GetOwner()->GetActorLocation();

    if (target_component) {
        location_b = target_component->GetComponentLocation();
        ab_direction = get_ab_direction();
        ba_direction = get_ba_direction();
        current_direction = ab_direction;
    } else {
        current_direction = FVector::ZeroVector;
    }
}

void ULoopingPlatformComponent::TickComponent(float DeltaTime,
                                              ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!target_component || current_direction.IsZero()) {
        return;
    }

    if (pause_timer <= pause_after_completion) {
        pause_timer += DeltaTime;
        return;
    }

    auto const current_location{GetOwner()->GetActorLocation()};
    auto const cur_going_to_b{going_to_b()};
    auto const target_location{cur_going_to_b ? location_b : location_a};
    auto const to_target{target_location - current_location};
    auto const distance_to_target{to_target.Size()};

    if (distance_to_target > (KINDA_SMALL_NUMBER)) {
        auto const move_step{FMath::Min(speed * DeltaTime, distance_to_target)};
        auto const move_vector{to_target.GetSafeNormal() * move_step};
        GetOwner()->AddActorWorldOffset(move_vector);
    } else {
        pause_timer = 0.0f;
        current_direction = cur_going_to_b ? ba_direction : ab_direction;
    }
}

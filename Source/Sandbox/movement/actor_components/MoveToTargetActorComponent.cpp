#include "Sandbox/actor_components/MoveToTargetActorComponent.h"

UMoveToTargetActorComponent::UMoveToTargetActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}
void UMoveToTargetActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        start_location = owner->GetActorLocation();
        if (!target_component) {
            target_component =
                Cast<USceneComponent>(owner->GetDefaultSubobjectByName(TEXT("TargetPoint")));
        }
        end_location = target_component->GetComponentLocation();
    }

    SetComponentTickEnabled(false);
}

void UMoveToTargetActorComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (auto* owner{GetOwner()}) {
        auto const current_location{owner->GetActorLocation()};
        auto const target_location{moving_forward ? end_location : start_location};
        auto const direction{(target_location - current_location).GetSafeNormal()};
        auto const step{move_speed * DeltaTime};
        auto const distance_to_target{FVector::Dist(current_location, target_location)};

        DrawDebugLine(
            GetWorld(), current_location, target_location, FColor::Green, false, -1.0f, 0, 2.0f);

        if (step >= distance_to_target) {
            owner->SetActorLocation(target_location);
            moving_forward = !moving_forward;
            SetComponentTickEnabled(false);
        } else {
            owner->SetActorLocation(current_location + direction * step);
        }
    }
}
void UMoveToTargetActorComponent::OnClicked() {
    if (auto* owner{GetOwner()}) {
        start_location = owner->GetActorLocation();
        if (target_component) {
            end_location = target_component->GetComponentLocation();
        }
    }

    SetComponentTickEnabled(true);
}

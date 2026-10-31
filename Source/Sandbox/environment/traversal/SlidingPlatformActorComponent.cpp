#include "Sandbox/environment/traversal/SlidingPlatformActorComponent.h"

#include <ioj/vector_value_math.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include "GameFramework/Actor.h"

USlidingPlatformActorComponent::USlidingPlatformActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void USlidingPlatformActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* owner{GetOwner()}) {
        auto const start{ioj::Vector3d{owner->GetActorLocation()}};
        travel_ = {.start = start,
                   .end = start + ioj::Vector3d{owner->GetActorForwardVector()} *
                                      static_cast<double>(move_distance)};
    }

    SetComponentTickEnabled(false);
}

void USlidingPlatformActorComponent::TickComponent(float DeltaTime,
                                                   ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (auto* owner{GetOwner()}) {
        auto const step{ioj::advance_linear_travel(
            travel_, ioj::Vector3d{owner->GetActorLocation()}, move_speed, DeltaTime)};
        owner->SetActorLocation(ioj::to_unreal(step.location));
        if (step.arrived) {
            SetComponentTickEnabled(false);
        }
    }
}
void USlidingPlatformActorComponent::OnClicked() {
    SetComponentTickEnabled(true);
}

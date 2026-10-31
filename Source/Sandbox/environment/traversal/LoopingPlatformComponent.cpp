#include "Sandbox/environment/traversal/LoopingPlatformComponent.h"

#include <SandboxCoreEngine/VectorConversion.h>

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

ULoopingPlatformComponent::ULoopingPlatformComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void ULoopingPlatformComponent::BeginPlay() {
    Super::BeginPlay();

    if (IsValid(target_component)) {
        travel_ = {.start = ioj::Vector3d{GetOwner()->GetActorLocation()},
                   .end = ioj::Vector3d{target_component->GetComponentLocation()}};
    }
}

void ULoopingPlatformComponent::TickComponent(float DeltaTime,
                                              ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsValid(target_component)) {
        return;
    }

    auto* const owner{GetOwner()};
    auto const step{ioj::advance_linear_travel(travel_,
                                               ioj::Vector3d{owner->GetActorLocation()},
                                               speed,
                                               DeltaTime,
                                               pause_after_completion)};
    owner->SetActorLocation(ioj::to_unreal(step.location));
}

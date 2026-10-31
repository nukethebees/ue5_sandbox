#include "Sandbox/environment/traversal/LoopingLiftComponent.h"

#include <ioj/vector_value_math.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include "GameFramework/Actor.h"

ULoopingLiftComponent::ULoopingLiftComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}

void ULoopingLiftComponent::BeginPlay() {
    Super::BeginPlay();
    auto const origin{ioj::Vector3d{GetOwner()->GetActorLocation()}};
    travel_ = {.start = origin,
               .end = origin +
                      ioj::safe_normal(ioj::Vector3d{direction}) * static_cast<double>(distance)};
}

void ULoopingLiftComponent::TickComponent(float DeltaTime,
                                          ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    auto* const owner{GetOwner()};
    auto const step{ioj::advance_linear_travel(travel_,
                                               ioj::Vector3d{owner->GetActorLocation()},
                                               speed,
                                               DeltaTime,
                                               pause_after_completion)};
    owner->SetActorLocation(ioj::to_unreal(step.location));
}

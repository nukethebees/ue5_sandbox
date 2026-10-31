#include "Sandbox/environment/traversal/LoopingLift.h"

#include <ioj/vector_value_math.h>

#include <SandboxCoreEngine/VectorConversion.h>

ALoopingLift::ALoopingLift() {
    PrimaryActorTick.bCanEverTick = true;
}
void ALoopingLift::BeginPlay() {
    Super::BeginPlay();
    auto const origin{ioj::Vector3d{GetActorLocation()}};
    travel_ = {.start = origin, .end = origin + ioj::Vector3d{0., 0., distance}};
}
void ALoopingLift::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    auto const step{
        ioj::advance_linear_travel(travel_, ioj::Vector3d{GetActorLocation()}, speed, DeltaTime)};
    SetActorLocation(ioj::to_unreal(step.location));
}

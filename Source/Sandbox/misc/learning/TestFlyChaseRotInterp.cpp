#include "TestFlyChaseRotInterp.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"

ATestFlyChaseRotInterp::ATestFlyChaseRotInterp() {}

void ATestFlyChaseRotInterp::Tick(float dt) {
    Super::Tick(dt);

    move_to_target(dt);
}

void ATestFlyChaseRotInterp::BeginPlay() {
    Super::BeginPlay();

    if (!target) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("target is nullptr"));
        SetActorTickEnabled(false);
    }
}

void ATestFlyChaseRotInterp::move_to_target(float dt) {
    auto const current_rotation{GetActorRotation()};
    auto const direction_to_target{
        (target->GetActorLocation() - GetActorLocation()).GetSafeNormal()};
    auto const rotation_to_target{direction_to_target.Rotation()};

    auto const new_rotation{
        FMath::RInterpConstantTo(current_rotation, rotation_to_target, dt, turn_speed_deg_per_s)};

    SetActorRotation(new_rotation);

    auto const pos{GetActorLocation()};
    auto const target_pos{target->GetActorLocation()};
    auto const max_delta_dist{speed * dt};

    SetActorLocation(pos + max_delta_dist * new_rotation.Vector());
}

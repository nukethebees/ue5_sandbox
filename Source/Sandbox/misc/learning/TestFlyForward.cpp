#include "TestFlyForward.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Engine/HitResult.h"
#include "Engine/World.h"

ATestFlyForward::ATestFlyForward() {}

void ATestFlyForward::Tick(float dt) {
    Super::Tick(dt);

    auto const direction{GetActorForwardVector()};
    auto const dpos{direction * speed * dt};
    auto const start{GetActorLocation()};
    auto const end{start + dpos};

    auto const hit{check_if_hit(start, end)};

    if (hit.bBlockingHit) {
        Destroy();
    } else {
        SetActorLocation(end);
    }
}

void ATestFlyForward::BeginPlay() {
    Super::BeginPlay();
}

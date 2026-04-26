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

    FHitResult hit{};
    FCollisionQueryParams params{};
    FCollisionObjectQueryParams obj_params{};
    params.AddIgnoredActor(this);

    auto* world{GetWorld()};

    bool const blocked{world->LineTraceSingleByObjectType(hit, start, end, obj_params, params)};

    if (blocked) {
        Destroy();
    } else {
        SetActorLocation(end);
    }
}

void ATestFlyForward::BeginPlay() {
    Super::BeginPlay();
}

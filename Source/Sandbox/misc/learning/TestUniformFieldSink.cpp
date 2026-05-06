#include "TestUniformFieldSink.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/world.h"
#include "TestUniformField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATestUniformFieldPointSink::ATestUniformFieldPointSink()
    : point_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    point_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldPointSink::Tick(float dt) {
    Super::Tick(dt);

    WARN_IF_EXPR_ELSE(!field) {
        auto const pos{GetActorLocation()};
        auto const sample{field->sample_field(pos)};

        auto const potential_mag{sample.potential.Size()};

        auto const new_speed{base_speed * potential_mag};
        auto const last_speed{speed};
        auto const delta_speed{new_speed - last_speed};

        auto const new_acceleration{delta_speed / dt};
        auto const clamped_acceleration{
            FMath::Clamp(new_acceleration, -max_acceleration, max_acceleration)};

        auto const clamped_speed{
            FMath::Clamp(last_speed + (clamped_acceleration * dt), -max_speed, max_speed)};
        auto const clamped_delta_pos{sample.potential.GetSafeNormal() * clamped_speed * dt};

        SetActorLocation(pos + FVector{clamped_delta_pos});

        speed = clamped_speed;
    }
}
void ATestUniformFieldPointSink::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformFieldPointSink::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}

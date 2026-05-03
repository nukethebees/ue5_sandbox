#include "TestUniformFieldSink.h"

#include "Sandbox/logging/SandboxLogCategories.h"
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

    if (auto field_ref{field.Pin()}) {
        auto const pos{GetActorLocation()};
        auto const sample{field_ref->sample_field(pos)};

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

        SetActorLocation(pos + clamped_delta_pos);

        speed = clamped_speed;
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to pin field."));
    }
}
void ATestUniformFieldPointSink::BeginPlay() {
    Super::BeginPlay();

    find_field();
}
void ATestUniformFieldPointSink::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    find_field();
}

void ATestUniformFieldPointSink::find_field() {
    field = ATestUniformField::find_field(*GetWorld());
}

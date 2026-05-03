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

        auto const delta_pos{sample.potential * speed * dt};
        SetActorLocation(pos + delta_pos);
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

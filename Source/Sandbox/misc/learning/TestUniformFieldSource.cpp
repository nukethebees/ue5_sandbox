#include "TestUniformFieldSource.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestUniformField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ATestUniformFieldPointSource::ATestUniformFieldPointSource()
    : point_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    point_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATestUniformFieldPointSource::Tick(float dt) {
    Super::Tick(dt);

    update_sources();
    broadcast_update_to_field();
}
void ATestUniformFieldPointSource::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformFieldPointSource::BeginPlay"));

    Super::BeginPlay();

    update_sources();
    find_field();
    broadcast_to_field();

    SetActorTickEnabled(use_tick);
}
void ATestUniformFieldPointSource::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    update_sources();
    find_field();
}

void ATestUniformFieldPointSource::update_sources() {
    for (auto& source : sources) {
        source.coordinate = GetActorLocation();
    }
}
void ATestUniformFieldPointSource::find_field() {
    field = ATestUniformField::find_field(*GetWorld());
}
void ATestUniformFieldPointSource::broadcast_to_field() {
    if (auto field_ref{field.Pin()}) {
        source_id = field_ref->add_sources(sources);
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to pin field."));
    }
}
void ATestUniformFieldPointSource::broadcast_update_to_field() {
    if (auto field_ref{field.Pin()}) {
        field_ref->update_sources(sources, source_id);
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to pin field."));
    }
}

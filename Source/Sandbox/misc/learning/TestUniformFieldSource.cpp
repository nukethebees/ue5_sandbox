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
    broadcast_to_field();
}
void ATestUniformFieldPointSource::BeginPlay() {
    Super::BeginPlay();

    update_sources();
    find_field();
    broadcast_to_field();
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
    if (!field.IsValid()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Field is not valid."));
        return;
    }

    if (auto field_ref{field.Pin()}) {
        for (auto& source : sources) {
            field_ref->add_source(source);
        }
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to pin field."));
    }
}

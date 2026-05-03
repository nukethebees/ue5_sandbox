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
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldPointSource::Tick(float dt) {
    Super::Tick(dt);

#if WITH_EDITOR
    dbg_log_timer += dt;
#endif

    update_source();
    broadcast_to_field();

#if WITH_EDITOR
    if (dbg_log_timer >= dbg_log_cooldown) {
        dbg_log_timer = 0.f;
    }
#endif
}
void ATestUniformFieldPointSource::BeginPlay() {
    Super::BeginPlay();
    find_field();
}
void ATestUniformFieldPointSource::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    update_source();
    find_field();
}
void ATestUniformFieldPointSource::update_source() {
    source.coordinate = GetActorLocation();
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
        field_ref->add_source(source);
    } else {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to pin field."));
    }
}

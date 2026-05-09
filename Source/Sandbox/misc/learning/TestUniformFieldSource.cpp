#include "TestUniformFieldSource.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
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

void ATestUniformFieldPointSource::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ATestUniformFieldPointSource::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformFieldPointSource::BeginPlay"));

    Super::BeginPlay();

    update_sources();

    WARN_IF_EXPR_ELSE(!field) {
        field->on_field_pre_construction.AddUObject(this, &ThisClass::on_field_pre_construction);
    }

    SetActorTickEnabled(use_tick);
}
void ATestUniformFieldPointSource::Tick(float dt) {
    Super::Tick(dt);

    update_sources();
    broadcast_update_to_field();
}
void ATestUniformFieldPointSource::EndPlay(EEndPlayReason::Type const reason) {
    WARN_IF_EXPR_ELSE(field == nullptr) {
        field->on_field_pre_construction.RemoveAll(this);
    }

    Super::EndPlay(reason);
}

void ATestUniformFieldPointSource::update_sources() {
    for (auto& source : sources) {
        source.coordinate = GetActorLocation();
    }
}
void ATestUniformFieldPointSource::broadcast_to_field(ATestUniformField& field_ref) {
    source_id = field_ref.add_dynamic_sources(sources);
}
void ATestUniformFieldPointSource::broadcast_to_field() {
    RETURN_IF_NULLPTR(field);
    broadcast_to_field(*field);
}
void ATestUniformFieldPointSource::broadcast_update_to_field(ATestUniformField& field_ref) {
    field_ref.update_dynamic_sources(sources, source_id);
}
void ATestUniformFieldPointSource::broadcast_update_to_field() {
    RETURN_IF_NULLPTR(field);
    broadcast_update_to_field(*field);
}
void ATestUniformFieldPointSource::on_field_pre_construction(ATestUniformField& field_ref) {
    UE_LOG(LogSandboxLearning,
           Verbose,
           TEXT("%s::on_field_pre_construction"),
           *ml::get_best_display_name(*this));

    broadcast_to_field(field_ref);
}

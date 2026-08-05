#include "TestUniformFieldDynSource.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "TestUniformField.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

ATestUniformFieldDynPointSource::ATestUniformFieldDynPointSource()
    : point_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    point_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldDynPointSource::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATestUniformFieldDynPointSource::BeginPlay"));

    Super::BeginPlay();

    reset_destination();
    last_update_pos = GetActorLocation();

    update_sources();

    if (field == nullptr) {
        SetActorTickEnabled(false);
        WARN_IS_FALSE(LogSandboxLearning, field);
        return;
    }

    field->on_field_pre_construction.AddUObject(this, &ThisClass::on_field_pre_construction);
}
void ATestUniformFieldDynPointSource::Tick(float dt) {
    Super::Tick(dt);

    log_cooldown.tick(dt);

    if (at_destination()) {
        if (enable_log_prints) {
            UE_LOG(LogSandboxLearning, Display, TEXT("At destination"));
        }

        update_destination();
    }

    move_to_destination(dt);

    if (should_update_sources()) {
        update_sources();
    }
    broadcast_update_to_field();

    if (log_cooldown.is_finished()) {
        log_cooldown.reset();
    }
}
void ATestUniformFieldDynPointSource::EndPlay(EEndPlayReason::Type const reason) {
    WARN_IF_EXPR_ELSE(field == nullptr) {
        field->on_field_pre_construction.RemoveAll(this);
    }

    Super::EndPlay(reason);
}

bool ATestUniformFieldDynPointSource::should_update_sources() const {
    return FVector::DistSquared(GetActorLocation(), last_update_pos) >
           (update_threshold * update_threshold);
}
void ATestUniformFieldDynPointSource::update_sources() {
    for (auto& source : sources) {
        source.coordinate = GetActorLocation();
    }
}
void ATestUniformFieldDynPointSource::broadcast_to_field() {
    RETURN_IF_NULLPTR(field);
    source_id = field->add_dynamic_sources(sources);
}
void ATestUniformFieldDynPointSource::broadcast_update_to_field() {
    RETURN_IF_NULLPTR(field);
    field->update_dynamic_sources(sources, source_id);
}
void ATestUniformFieldDynPointSource::on_field_pre_construction(ATestUniformField& field_ref) {
    if (enable_log_prints) {
        UE_LOG(LogSandboxLearning,
               Verbose,
               TEXT("%s::on_field_pre_construction"),
               *ml::get_best_display_name(*this));
    }

    check(&field_ref == &*field);

    broadcast_to_field();
}

void ATestUniformFieldDynPointSource::reset_destination() {
    destination = GetActorLocation();
}
bool ATestUniformFieldDynPointSource::at_destination() {
    return FVector::DistSquared(GetActorLocation(), destination) <
           (acceptance_radius * acceptance_radius);
}
void ATestUniformFieldDynPointSource::update_destination() {
    auto const origin{field->get_field_centre()};
    auto const extents{field->get_field_extent()};

    destination = UKismetMathLibrary::RandomPointInBoundingBox(origin, extents);

    if (enable_log_prints) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("Set new destination: %s (origin: %s, extents: %s)"),
               *destination.ToCompactString(),
               *origin.ToCompactString(),
               *extents.ToCompactString());
    }
}
void ATestUniformFieldDynPointSource::move_to_destination(float dt) {
    auto const pos{GetActorLocation()};
    auto const dir{(destination - pos).GetSafeNormal()};

    SetActorLocation(GetActorLocation() + dir * speed * dt);
}

bool ATestUniformFieldDynPointSource::can_log() const {
    return enable_log_prints && log_cooldown.is_finished();
}

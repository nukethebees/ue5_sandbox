#include "TestUniformFieldFly0.h"

#include <Sandbox/utilities/actor_utils.h>
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "TestUniformField.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>

ATestUniformFieldFly0::ATestUniformFieldFly0()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestUniformFieldFly0::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ATestUniformFieldFly0::BeginPlay() {
    Super::BeginPlay();

    arrival_cooldown.finish();
    log_cooldown.finish();

    if (enable_log_prints) {
        UE_LOG(LogSandboxLearning, Display, TEXT("ATestUniformFieldFly0::BeginPlay"));
    }

    // Force a new destination on the first tick
    destination = GetActorLocation();

    if (!assert_field_exists()) {
        return;
    }

    if (!ml::actor_is_within(*this, *field, false)) {
        UE_LOG(LogSandboxLearning,
               Error,
               TEXT("%s is not within the vector field"),
               *ml::get_best_display_name(*this));
        SetActorTickEnabled(false);
        return;
    }
}
void ATestUniformFieldFly0::Tick(float dt) {
    Super::Tick(dt);

    log_cooldown -= dt;
    arrival_cooldown -= dt;

    if (can_log()) {
        UE_LOG(LogSandboxLearning, Display, TEXT("ATestUniformFieldFly0::Tick"));
    }

    if (!assert_field_exists()) {
        return;
    }

    if (show_destination) {
        display_destination();
    }

    if (arrival_cooldown.is_finished()) {
        if (at_target()) {
            if (enable_log_prints) {
                UE_LOG(LogSandboxLearning, Display, TEXT("At the target."));
            }
            set_new_destination();
        } else {
            move_to_destination(dt);
        }

        handle_oob();
    }

    if (log_cooldown.is_finished()) {
        log_cooldown.reset();
    }
}
void ATestUniformFieldFly0::EndPlay(EEndPlayReason::Type const reason) {
    Super::EndPlay(reason);
}

void ATestUniformFieldFly0::set_new_destination() {
    if (enable_log_prints) {
        UE_LOG(LogSandboxLearning, Display, TEXT("ATestUniformFieldFly0::set_new_destination"));
    }

    auto const pos{GetActorLocation()};
    auto const sample{field->sample_field(pos).potential};

    auto const sample_direction{sample.GetSafeNormal()};

    auto const dist_to_move{FMath::FRandRange(min_dist, max_dist)};

    if (can_log()) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Dist to move: %.2f"), dist_to_move);
    }

    destination = FVector{
        pos.X + (sample_direction.X * dist_to_move),
        pos.Y + (sample_direction.Y * dist_to_move),
        pos.Z + (sample_direction.Z * dist_to_move),
    };

    if (enable_log_prints) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("New destination: %s (dist=%.f)"),
               *destination.ToCompactString(),
               FVector::Dist(pos, destination));
    }

    auto const direction{destination - pos};

    SetActorRotation(direction.Rotation());
}
void ATestUniformFieldFly0::move_to_destination(float dt) {
    auto const old_pos{GetActorLocation()};
    auto const delta_dist{dt * speed};
    auto const delta_pos{GetActorForwardVector() * delta_dist};
    auto const new_pos{old_pos + delta_pos};

    SetActorLocation(new_pos);
}
bool ATestUniformFieldFly0::at_target() const {
    return FVector::DistSquared(GetActorLocation(), destination) <=
           (acceptance_radius * acceptance_radius);
}
void ATestUniformFieldFly0::display_destination() {
    TRY_INIT_PTR(world, GetWorld());

    constexpr float thickness{8.f};
    auto const colour{FColor::Green};

    DrawDebugLine(world, GetActorLocation(), destination, colour, false, 0.f, 0u, thickness);
    DrawDebugSphere(world, destination, acceptance_radius, 8, colour, false, 0.f, 0u, thickness);
}
void ATestUniformFieldFly0::handle_oob() {
    if (!ml::actor_is_within(*this, *field, false)) {
        auto const field_pos{field->GetActorLocation()};
        SetActorLocation(field_pos);
        destination = field_pos;
    }
}
bool ATestUniformFieldFly0::assert_field_exists() {
    if (field == nullptr) {
        WARN_IS_FALSE(LogSandboxLearning, field);
        SetActorTickEnabled(false);
        return false;
    }

    return true;
}

bool ATestUniformFieldFly0::can_log() const {
    return enable_log_prints && log_cooldown.is_finished();
}

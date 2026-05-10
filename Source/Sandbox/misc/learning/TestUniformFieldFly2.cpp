#include "TestUniformFieldFly2.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"
#include "TestUniformField.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/World.h>

ATestUniformFieldFly2::ATestUniformFieldFly2()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

// Lifecycle
void ATestUniformFieldFly2::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}
void ATestUniformFieldFly2::BeginPlay() {
    Super::BeginPlay();

    set_state(ETestUniformFieldFly2State::exploring);

    log_config.reset();

    if (can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("ATestUniformFieldFly2::BeginPlay"));
    }

    // Force a new destination on the first tick
    destination = GetActorLocation();
    target = nullptr;

    if (!assert_field_exists()) {
        SetActorTickEnabled(false);
        return;
    }

    if (!laser_class) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("laser_class is nullptr"));
        SetActorTickEnabled(false);
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
void ATestUniformFieldFly2::Tick(float dt) {
    Super::Tick(dt);

    log_config.tick(dt);
    fire_cooldown.tick(dt);

    switch (state) {
        case ETestUniformFieldFly2State::exploring: {
            explore(dt);
            break;
        }
        case ETestUniformFieldFly2State::tracking: {
            track(dt);
            break;
        }
    }

    log_config.on_tick_end();
}
void ATestUniformFieldFly2::EndPlay(EEndPlayReason::Type const reason) {
    Super::EndPlay(reason);
}

// State
void ATestUniformFieldFly2::set_state(ETestUniformFieldFly2State new_state) {
    state = new_state;

    switch (state) {
        case ETestUniformFieldFly2State::exploring: {
            destination = GetActorLocation();
            target = nullptr;
            break;
        }
        case ETestUniformFieldFly2State::tracking: {
            break;
        }
    }
}
void ATestUniformFieldFly2::explore(float dt) {
    if (show_vision_cone) {
        display_vision_cone();
    }

    if (at_target()) {
        set_new_destination();
    }

    ml::face_point(*this, destination);
    move_to_destination(dt);

    if (try_find_target()) {
        set_state(ETestUniformFieldFly2State::tracking);
    }
}
void ATestUniformFieldFly2::track(float dt) {
    if (!target.IsValid()) {
        // Lost the target
        set_state(ETestUniformFieldFly2State::exploring);
        return;
    }

    ml::face_actor(*this, *target);
    move_to_destination(dt);

    TRY_INIT_PTR(world, GetWorld());

    FVector const fire_point{GetActorLocation() + GetActorForwardVector() * fire_point_distance};
    FTransform const laser_transform{
        GetActorRotation(),
        fire_point,
        FVector::OneVector,
    };

    if (fire_cooldown.reset_if_done()) {
        TRY_INIT_PTR(
            laser,
            world->SpawnActorDeferred<AShipLaser>(laser_class, laser_transform, nullptr, this));
        laser->FinishSpawning(laser_transform);
    }
}

// Navigation
void ATestUniformFieldFly2::set_new_destination() {
    if (can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("ATestUniformFieldFly2::set_new_destination"));
    }

    auto const pos{GetActorLocation()};

    auto const sample{[&] -> FVector3f {
        if (is_oob()) {
            return FVector3f{field->GetActorLocation() - pos};
        } else {
            return field->sample_field(pos).potential;
        }
    }()};

    if (sample.IsNearlyZero()) {
        if (can_log(EActorLoggingVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning,
                   Warning,
                   TEXT("%s: Sample is nearly zero"),
                   *ml::get_best_display_name(*this));
        }
        return;
    }

    auto const sample_direction{sample.GetSafeNormal()};

    auto const dist_to_move{dist_bounds.get_rand()};

    if (can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Dist to move: %.2f"), dist_to_move);
    }

    destination = FVector{
        pos.X + (sample_direction.X * dist_to_move),
        pos.Y + (sample_direction.Y * dist_to_move),
        pos.Z + (sample_direction.Z * dist_to_move),
    };

    if (can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("New destination: %s (dist=%.f)"),
               *destination.ToCompactString(),
               FVector::Dist(pos, destination));
    }

    auto const direction{destination - pos};

    SetActorRotation(direction.Rotation());
}
void ATestUniformFieldFly2::move_to_destination(float dt) {
    auto const old_pos{GetActorLocation()};
    auto const delta_dist{dt * speed};
    auto const delta_pos{GetActorForwardVector() * delta_dist};
    auto const new_pos{old_pos + delta_pos};

    SetActorLocation(new_pos);
}
bool ATestUniformFieldFly2::at_target() const {
    return FVector::DistSquared(GetActorLocation(), destination) <=
           (acceptance_radius * acceptance_radius);
}
void ATestUniformFieldFly2::display_destination() {
    TRY_INIT_PTR(world, GetWorld());

    constexpr float thickness{8.f};
    auto const colour{FColor::Green};

    DrawDebugLine(world, GetActorLocation(), destination, colour, false, 0.f, 0u, thickness);
    DrawDebugSphere(world, destination, acceptance_radius, 8, colour, false, 0.f, 0u, thickness);
}
bool ATestUniformFieldFly2::is_oob() const {
    return !ml::actor_is_within(*this, *field, false);
}

// Vision
void ATestUniformFieldFly2::display_vision_cone() {
    auto const angle{FMath::DegreesToRadians(vision_angle_deg / 2.f)};

    DrawDebugCone(GetWorld(),
                  GetActorLocation(),
                  GetActorForwardVector(),
                  vision_radius,
                  angle,
                  angle,
                  16,
                  FColor::Orange,
                  false,
                  0.f,
                  0u,
                  8.f);
}

// Field
bool ATestUniformFieldFly2::assert_field_exists() {
    if (field == nullptr) {
        WARN_IS_FALSE(LogSandboxLearning, field);
        SetActorTickEnabled(false);
        return false;
    }

    return true;
}

// Targets
bool ATestUniformFieldFly2::try_find_target() {
    if (can_log(EActorLoggingVerbosity::Verbose)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("%s: Finding a new target"),
               *ml::get_best_display_name(*this));
    }

    FVector origin;
    FVector extent;

    GetActorBounds(false, origin, extent);

    auto const hits{ml::find_actors_within_cone(*this,
                                                vision_radius,
                                                extent.Z * 2.0,
                                                FMath::RadiansToDegrees(vision_angle_deg / 2.f),
                                                target_classes)};

    // Get the one closest to the centre
    auto const num_hits{hits.Num()};
    if (num_hits == 0) {
        return false;
    }

    target = hits[0];

    if (num_hits == 1) {
        return true;
    } else {
        auto const pos{GetActorLocation()};
        auto const fwd{GetActorForwardVector()};
        auto target_angle{ml::get_abs_angle_from_fwd_vector(pos, fwd, *target)};
        for (int32 i{1}; i < num_hits; ++i) {
            auto* hit_actor{hits[i]};
            auto const hit_angle{ml::get_abs_angle_from_fwd_vector(pos, fwd, *hit_actor)};

            if (hit_angle < target_angle) {
                target = hit_actor;
                target_angle = hit_angle;
            }
        }
    }

    if (can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Found: %s"), *ml::get_best_display_name(*target));
    }

    return true;
}

// Logging
bool ATestUniformFieldFly2::can_log(EActorLoggingVerbosity msg_verbosity) const {
    return log_config.can_log(msg_verbosity);
}
bool ATestUniformFieldFly2::can_tick_log(EActorLoggingVerbosity msg_verbosity) const {
    return log_config.can_tick_log(msg_verbosity);
}

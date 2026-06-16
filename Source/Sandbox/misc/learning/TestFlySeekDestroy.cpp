#include "TestFlySeekDestroy.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestVolume.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"

#include <Components/BoxComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <Kismet/KismetMathLibrary.h>

ATestFlySeekDestroy::ATestFlySeekDestroy() {}

void ATestFlySeekDestroy::OnConstruction(FTransform const& t) {
    Super::OnConstruction(t);

    transition_to_state();
}
void ATestFlySeekDestroy::BeginPlay() {
    Super::BeginPlay();

    if (!search_state.search_volume) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("search_volume is nullptr"));
        SetActorTickEnabled(false);
    }

    TRY_INIT_PTR(world, GetWorld());

    search_state.draw_config.world = world;
    chase_state.draw_config.world = world;
    attack_state.draw_config.world = world;

    set_state(ETestFlySeekDestroyState::searching);

    search_state.draw_config.cone_angle_half_width_deg = vision.half_angle_deg;
    search_state.draw_config.cone_angle_half_height_deg = vision.half_angle_deg;
}
void ATestFlySeekDestroy::Tick(float dt) {
    Super::Tick(dt);

    attack_state.burst.tick(dt);

    switch (state) {
        case ETestFlySeekDestroyState::searching: {
            handle_search(dt);
            break;
        }
        case ETestFlySeekDestroyState::chasing: {
            handle_chase(dt);
            break;
        }
        case ETestFlySeekDestroyState::attacking: {
            handle_attack(dt);
            break;
        }
    }

    if (show_debug_shapes) { draw_debug_shapes(); }
}

// Movement
auto ATestFlySeekDestroy::within_radius(FVector const& point, float const r) const -> bool {
    return FVector::DistSquared(point, GetActorLocation()) <= (r * r);
}

// State
void ATestFlySeekDestroy::set_state(ETestFlySeekDestroyState new_state) {
    state = new_state;
    transition_to_state();
}
void ATestFlySeekDestroy::transition_to_state() {
    switch (state) {
        case ETestFlySeekDestroyState::searching: {
            assign_state_data(search_state);

            search_state.destination = GetActorLocation();
            target_state.target.Reset();

            break;
        }
        case ETestFlySeekDestroyState::chasing: {
            assign_state_data(chase_state);
            break;
        }
        case ETestFlySeekDestroyState::attacking: {
            assign_state_data(attack_state);
            break;
        }
    }

    material_state.update_instance();
}
template <typename T>
void ATestFlySeekDestroy::assign_state_data(T const& state_data) {
    material_state.config = state_data.material_config;
    speed = state_data.speed;
}

// Search
void ATestFlySeekDestroy::handle_search(float dt) {
    if (scan_for_target()) {
        set_state(ETestFlySeekDestroyState::chasing);
        return handle_chase(dt);
    }

    if (within_radius(search_state.destination, search_state.acceptance_radius)) {
        if (log_config.can_log(EActorLogVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning,
                   Display,
                   TEXT("(%s) At destination. Finding new destination."),
                   *ml::get_best_display_name(*this));
        }

        set_new_search_destination();
    }

    move_to_location(dt, search_state.destination);
}
void ATestFlySeekDestroy::set_new_search_destination() {
    auto* box{search_state.search_volume->get_box()};
    auto const box_pos{box->GetComponentLocation()};
    auto const box_extent{box->GetScaledBoxExtent()};

    while (within_radius(search_state.destination, search_state.min_distance_to_new_point)) {
        search_state.destination =
            UKismetMathLibrary::RandomPointInBoundingBox(box_pos, box_extent);
    }

    if (log_config.can_log(EActorLogVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("(%s) New destination: %s"),
               *ml::get_best_display_name(*this),
               *search_state.destination.ToCompactString());
    }
}
auto ATestFlySeekDestroy::scan_for_target() -> bool {
    FVector origin;
    FVector extent;

    GetActorBounds(false, origin, extent);

    auto hits{ml::find_actors_within_cone(
        *this, vision.radius, extent.Z * 2.0, vision.half_angle_rad(), ThisClass::StaticClass())};

    target_state.target = ml::get_centre_actor_in_fov(*this, hits);
    if (!target_state.target.IsValid()) { return false; }

    return true;
}

// Chase
void ATestFlySeekDestroy::handle_chase(float dt) {
    if (!target_state.target.IsValid()) {
        set_state(ETestFlySeekDestroyState::searching);
        return;
    }

    auto const tgt_pos{target_state.target->GetActorLocation()};

    if (within_radius(tgt_pos, chase_state.acceptance_radius)) {
        set_state(ETestFlySeekDestroyState::attacking);
        return handle_attack(dt);
    }

    move_to_location(dt, tgt_pos);
}
void ATestFlySeekDestroy::move_to_location(float dt, FVector const& location) {
    auto const current_rotation{GetActorRotation()};
    auto const direction_to_target{(location - GetActorLocation()).GetSafeNormal()};
    auto const rotation_to_target{direction_to_target.Rotation()};

    auto const new_rotation{
        FMath::RInterpConstantTo(current_rotation, rotation_to_target, dt, turn_speed_deg_per_s)};

    SetActorRotation(new_rotation);

    auto const pos{GetActorLocation()};
    auto const max_delta_dist{speed * dt};

    SetActorLocation(pos + max_delta_dist * new_rotation.Vector());
}

// Attack
void ATestFlySeekDestroy::handle_attack(float dt) {
    if (!target_state.target.IsValid()) {
        set_state(ETestFlySeekDestroyState::searching);
        return;
    }

    auto const pos{GetActorLocation()};
    auto const target_pos{target_state.target->GetActorLocation()};

    move_to_location(dt, target_pos);

    auto const cos_threshold{
        FMath::Cos(FMath::DegreesToRadians(attack_state.max_fire_angle_degrees))};
    auto const to_enemy{(target_pos - pos).GetSafeNormal()};
    auto const forward{GetActorForwardVector()};
    auto const can_fire{FVector::DotProduct(forward, to_enemy) >= cos_threshold};

    if (can_fire) { fire_laser(); }
}
void ATestFlySeekDestroy::fire_laser() {
    if (!attack_state.burst.can_fire()) { return; }

    TRY_INIT_PTR(world, GetWorld());

    FVector const fire_point{GetActorLocation() +
                             GetActorForwardVector() * attack_state.fire_point_distance};

    FTransform const laser_transform{
        GetActorForwardVector().Rotation(),
        fire_point,
        FVector::OneVector,
    };

    TRY_INIT_PTR(laser,
                 world->SpawnActorDeferred<AShipLaser>(
                     attack_state.laser_class, laser_transform, this, this));
    laser->set_damage(attack_state.laser_damage);
    laser->FinishSpawning(laser_transform);

    attack_state.burst.fire();
}

void ATestFlySeekDestroy::draw_debug_shapes() {
    TRY_INIT_PTR(world, GetWorld());
    auto const pos{GetActorLocation()};

    switch (state) {
        case ETestFlySeekDestroyState::searching: {
            search_state.draw_config.draw_sphere(pos, search_state.acceptance_radius);

            search_state.draw_config.draw_line(pos, search_state.destination);
            search_state.draw_config.draw_cone(pos, GetActorForwardVector(), vision.radius);

            search_state.draw_config.draw_sphere(search_state.destination,
                                                 search_state.destination_sphere_radius);

            break;
        }
        case ETestFlySeekDestroyState::chasing: {
            chase_state.draw_config.draw_arrow(pos, target_state.target->GetActorLocation());
            chase_state.draw_config.draw_sphere(pos, chase_state.acceptance_radius);
            break;
        }
        case ETestFlySeekDestroyState::attacking: {
            attack_state.draw_config.draw_arrow(pos, target_state.target->GetActorLocation());
            break;
        }
    }
}

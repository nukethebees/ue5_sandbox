#include "TestFlySeekDestroyEvade.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestVolume.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/enums.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/spatial.h"
#include "Sandbox/utilities/vision_maths.h"

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Engine/World.h>
#include <Kismet/KismetMathLibrary.h>

ATestFlySeekDestroyEvade::ATestFlySeekDestroyEvade()
    : fire_point{CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"))} {

    fire_point->SetupAttachment(RootComponent);
}

void ATestFlySeekDestroyEvade::OnConstruction(FTransform const& t) {
    Super::OnConstruction(t);

    transition_to_state(state);
}
void ATestFlySeekDestroyEvade::BeginPlay() {
    Super::BeginPlay();

    if (!config.search.search_volume) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("search_volume is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (config.randomise_at_begin_play) {
        SetActorLocation(get_random_position_in_volume(GetActorLocation(), 1.f));
    }

    burst.reset();
    stale_attack_timeout.reset();

    set_state(ETestFlySeekDestroyEvadeState::searching);

    TRY_INIT_PTR(world, GetWorld());

    config.search.visuals_config.debug_drawer.world = world;
    config.chase.visuals_config.debug_drawer.world = world;
    config.attack.visuals_config.debug_drawer.world = world;
    config.attack_reposition.visuals_config.debug_drawer.world = world;
    config.evade.visuals_config.debug_drawer.world = world;

    config.search.visuals_config.debug_drawer.cone_angle_half_width_deg =
        config.vision.half_angle_deg;
    config.search.visuals_config.debug_drawer.cone_angle_half_height_deg =
        config.vision.half_angle_deg;
}
void ATestFlySeekDestroyEvade::Tick(float dt) {
    Super::Tick(dt);

    burst.tick(dt);
    stale_attack_timeout.tick(dt);

    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            handle_search(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            handle_chase(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            handle_attack(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attack_repositioning: {
            handle_attack_repositioning(dt);
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            handle_evade(dt);
            break;
        }
    }

    if (show_debug_shapes) { draw_debug_shapes(); }

    if (target.IsValid()) {
        target_previous_position_0 = target_previous_position_1;
        target_previous_position_1 = target->GetActorLocation();
    }
}

void ATestFlySeekDestroyEvade::set_config(FTestFlySeekDestroyEvadeConfig const& new_config) {
    config = new_config;
}
void ATestFlySeekDestroyEvade::set_movement(
    FTestFlySeekDestroyEvadeMovementConfig const& new_config) {
    turn_speed_deg_per_s = new_config.turn_speed_deg_per_s;
    speed = new_config.speed;
}

void ATestFlySeekDestroyEvade::set_state(ETestFlySeekDestroyEvadeState new_state) {
    auto const old_state{state};
    state = new_state;
    transition_to_state(old_state);
}
void ATestFlySeekDestroyEvade::transition_to_state(ETestFlySeekDestroyEvadeState const old_state) {
    if (log_config.can_log(EActorLogVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("Moving from %s to %s"),
               *ml::to_string_without_type_prefix(old_state),
               *ml::to_string_without_type_prefix(state));
    }

    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            material_state.config = config.search.visuals_config.material;

            set_movement(config.search.movement);
            destination = GetActorLocation();
            target.Reset();
            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            material_state.config = config.chase.visuals_config.material;

            set_movement(config.chase.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attack_repositioning: {
            material_state.config = config.attack.visuals_config.material;
            set_movement(config.attack.movement);
            set_reposition_destination();
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            stale_attack_timeout.reset();
            material_state.config = config.attack.visuals_config.material;
            set_movement(config.attack.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            material_state.config = config.evade.visuals_config.material;

            set_movement(config.evade.movement);
            break;
        }
    }

    material_state.update_instance();
}

void ATestFlySeekDestroyEvade::handle_search(float dt) {
    if (scan_for_target()) {
        if (log_config.can_log(EActorLogVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning,
                   Display,
                   TEXT("Found target: %s"),
                   *ml::get_best_display_name(*target));
        }

        set_state(ETestFlySeekDestroyEvadeState::chasing);
        return handle_chase(dt);
    }

    if (within_radius(GetActorLocation(), destination, config.search.acceptance_radius)) {
        destination = get_random_position_in_volume(GetActorLocation(),
                                                    config.search.min_distance_to_new_point);
    }

    move_to_location(dt, destination);
}
auto ATestFlySeekDestroyEvade::scan_for_target() -> bool {
    FVector origin;
    FVector extent;

    GetActorBounds(false, origin, extent);

    auto hits{ml::find_actors_within_cone(*this,
                                          config.vision.radius,
                                          extent.Z * 2.0,
                                          config.vision.half_angle_rad(),
                                          ThisClass::StaticClass())};

    target = ml::get_centre_actor_in_fov(*this, hits);
    if (!target.IsValid()) {
        target.Reset();
        return false;
    }
    target_previous_position_0 = target->GetActorLocation();
    target_previous_position_1 = target->GetActorLocation();

    return true;
}

void ATestFlySeekDestroyEvade::handle_chase(float dt) {
    if (!validate_target()) {
        set_state(ETestFlySeekDestroyEvadeState::searching);
        handle_search(dt);
        return;
    }

    auto const target_pos{target->GetActorLocation()};

    if (within_radius(GetActorLocation(), target_pos, config.chase.acceptance_radius)) {
        set_state(ETestFlySeekDestroyEvadeState::attacking);
        return handle_attack(dt);
    }

    move_to_location(dt, target_pos);
}

void ATestFlySeekDestroyEvade::handle_attack(float dt) {
    if (!validate_target()) {
        set_state(ETestFlySeekDestroyEvadeState::searching);
        handle_search(dt);
        return;
    }

    // If you haven't attacked in a while, reposition
    if (stale_attack_timeout.reset_if_done()) {
        set_state(ETestFlySeekDestroyEvadeState::attack_repositioning);
        handle_attack_repositioning(dt);
        return;
    }

    auto const pos{GetActorLocation()};
    auto const target_pos{target->GetActorLocation()};

    if (within_radius(GetActorLocation(), target_pos, config.attack.reposition_radius)) {
        if (log_config.can_log(EActorLogVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning, Display, TEXT("Too close to target, repositioning."));
        }

        set_state(ETestFlySeekDestroyEvadeState::attack_repositioning);
        handle_attack_repositioning(dt);
        return;
    }

    auto const attack_position{get_attack_position(dt)};
    destination = attack_position;
    move_to_location(dt, attack_position);

    auto const cos_threshold{
        FMath::Cos(FMath::DegreesToRadians(config.attack.max_fire_angle_degrees))};
    auto const to_attack_pos{(attack_position - pos).GetSafeNormal()};
    auto const forward{GetActorForwardVector()};
    auto const can_fire{FVector::DotProduct(forward, to_attack_pos) >= cos_threshold};

    if (can_fire) { fire_laser(); }
}
auto ATestFlySeekDestroyEvade::get_attack_position(float dt) const -> FVector {
    auto const target_pos{target->GetActorLocation()};
    auto const target_fwd{target->GetActorForwardVector()};

    // Work out the attack position
    // Do a rough version for now
    auto const target_delta_pos{target_previous_position_1 - target_previous_position_0};
    auto const target_velocity{FMath::IsNearlyZero(dt) ? FVector::ZeroVector
                                                       : target_delta_pos / dt};

    auto const* laser_cdo{config.attack.laser_class->GetDefaultObject<AShipLaser>()};
    auto const laser_speed{laser_cdo->get_speed()};

    auto const pos{GetActorLocation()};
    auto const dist{FVector::Dist(pos, target_pos)};
    // speed = m/s
    // dist = m
    // time = dist / speed

    auto const intercept_time{dist / laser_speed};
    auto const new_target_pos{target_pos + target_velocity * intercept_time};

    // Second time to reduce the error
    auto const dist_2{FVector::Dist(pos, new_target_pos)};

    auto const intercept_time_2{dist_2 / laser_speed};
    auto const new_target_pos_2{target_pos + target_velocity * intercept_time};

    return new_target_pos_2;
}
void ATestFlySeekDestroyEvade::fire_laser() {
    if (!burst.can_fire()) { return; }

    if (log_config.can_log(EActorLogVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Firing laser"));
    }

    TRY_INIT_PTR(world, GetWorld());

    FTransform const laser_transform{
        fire_point->GetComponentRotation(),
        fire_point->GetComponentLocation(),
        FVector::OneVector,
    };

    TRY_INIT_PTR(laser,
                 world->SpawnActorDeferred<AShipLaser>(
                     config.attack.laser_class, laser_transform, this, this));
    laser->set_damage(config.attack.laser_damage);
    laser->FinishSpawning(laser_transform);

    burst.fire();
    stale_attack_timeout.reset();
}

void ATestFlySeekDestroyEvade::handle_attack_repositioning(float dt) {
    if (!validate_target()) {
        set_state(ETestFlySeekDestroyEvadeState::searching);
        handle_search(dt);
        return;
    }

    auto const loc{GetActorLocation()};

    if (within_radius(loc, destination, config.attack_reposition.acceptance_radius)) {
        set_state(ETestFlySeekDestroyEvadeState::chasing);
    }

    // Attack again if the target moves far enough away
    if (!within_radius(
            loc, target->GetActorLocation(), config.attack_reposition.target_threshold_distance)) {
        set_state(ETestFlySeekDestroyEvadeState::attacking);
    }

    move_to_location(dt, destination);
}
void ATestFlySeekDestroyEvade::set_reposition_destination() {
    destination = get_random_position(GetActorLocation(),
                                      config.attack_reposition.new_position_bounds.min,
                                      config.attack_reposition.new_position_bounds.max);
}

void ATestFlySeekDestroyEvade::handle_evade(float dt) {
    if (within_radius(GetActorLocation(), destination, config.search.acceptance_radius)) {
        set_state(ETestFlySeekDestroyEvadeState::searching);
        handle_search(dt);
        return;
    }

    move_to_location(dt, destination);
}

// Movement
auto ATestFlySeekDestroyEvade::get_random_position_in_volume(FVector const& reference,
                                                             float min_dist) const -> FVector {
    auto* box{config.search.search_volume->get_box()};
    auto const box_pos{box->GetComponentLocation()};
    FVector const box_extent{box->GetScaledBoxExtent()};

    auto pos{UKismetMathLibrary::RandomPointInBoundingBox(box_pos, box_extent)};
    int32 count{1};
    int32 limit{100};
    while (within_radius(reference, pos, min_dist)) {
        pos = UKismetMathLibrary::RandomPointInBoundingBox(box_pos, box_extent);
        count++;

        if (count > limit) {
            UE_LOG(LogSandboxLearning, Warning, TEXT("Failed to find point: (min=%.2f)"), min_dist);
            return pos;
        }
    }

    return pos;
}
auto ATestFlySeekDestroyEvade::get_random_position(FVector const& reference,
                                                   float min_dist,
                                                   float max_dist) const -> FVector {
    return ml::get_random_point(reference, min_dist, max_dist);
}

void ATestFlySeekDestroyEvade::move_to_location(float dt, FVector const& location) {
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
auto ATestFlySeekDestroyEvade::within_radius(FVector const& point_a,
                                             FVector const& point_b,
                                             float const r) const -> bool {
    return FVector::DistSquared(point_a, point_b) <= (r * r);
}
auto ATestFlySeekDestroyEvade::within_bounds(FVector const& point_a,
                                             FVector const& point_b,
                                             float const min_bound,
                                             float const max_bound) const -> bool {
    check(min_bound >= 0.f);
    check(max_bound >= 0.f);
    check(max_bound > min_bound);

    auto const min_sq{min_bound * min_bound};
    auto const max_sq{max_bound * max_bound};
    auto const dist_sq{FVector::DistSquared(point_a, point_b)};

    return (dist_sq >= min_sq) && (dist_sq <= max_sq);
}

auto ATestFlySeekDestroyEvade::apply_damage(ShipDamageContext context) -> FShipDamageResult {
    auto const result{Super::apply_damage(context)};

    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            set_state(ETestFlySeekDestroyEvadeState::evading);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attack_repositioning:
            [[fallthrough]];
        case ETestFlySeekDestroyEvadeState::chasing: {
            if (target.IsValid() && (&context.instigator == target.Get())) {
                set_state(ETestFlySeekDestroyEvadeState::attacking);
            }
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            // Change direction
            destination = get_random_position(GetActorLocation(),
                                              config.evade.new_position_bounds.min,
                                              config.evade.new_position_bounds.max);
        }
        default: {
            break;
        }
    }

    return result;
}

void ATestFlySeekDestroyEvade::draw_debug_shapes() {
    TRY_INIT_PTR(world, GetWorld());
    auto const pos{GetActorLocation()};

    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            auto const& drawer{config.search.visuals_config.debug_drawer};

            drawer.draw_sphere(pos, config.search.acceptance_radius);

            drawer.draw_line(pos, destination);
            drawer.draw_cone(pos, GetActorForwardVector(), config.vision.radius);

            drawer.draw_sphere(destination, config.search.acceptance_radius);

            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            auto const& drawer{config.chase.visuals_config.debug_drawer};

            if (target.IsValid()) {
                auto const target_pos{target->GetActorLocation()};
                drawer.draw_arrow(pos, target_pos);
                drawer.draw_sphere(target_pos, config.chase.acceptance_radius);
            }

            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            auto const& drawer{config.attack.visuals_config.debug_drawer};

            drawer.draw_arrow(pos, destination);

            // Show where we're trying to attack
            drawer.draw_sphere(destination, config.attack.target_sphere_radius);
            if (target.IsValid()) {
                drawer.draw_sphere(target->GetActorLocation(), config.attack.reposition_radius);
            }

            break;
        }
        case ETestFlySeekDestroyEvadeState::attack_repositioning: {
            auto const& drawer{config.attack_reposition.visuals_config.debug_drawer};

            drawer.draw_sphere(destination, config.attack_reposition.acceptance_radius);
            drawer.draw_line(pos, destination);

            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            auto const& drawer{config.evade.visuals_config.debug_drawer};

            break;
        }
    }
}

void ATestFlySeekDestroyEvade::log_target_not_valid() {
    if (log_config.can_log(EActorLogVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Target is no longer valid."));
    }
}

auto ATestFlySeekDestroyEvade::validate_target() -> bool {
    if (!target.IsValid()) {
        target.Reset();
        log_target_not_valid();
        return false;
    }

    return true;
}

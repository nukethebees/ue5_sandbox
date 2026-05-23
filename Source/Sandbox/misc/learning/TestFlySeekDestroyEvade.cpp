#include "TestFlySeekDestroyEvade.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestVolume.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/enums.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
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

    if (show_debug_shapes) {
        draw_debug_shapes();
    }

    if (target.IsValid()) {
        target_previous_position = target->GetActorLocation();
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
    if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
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
}

void ATestFlySeekDestroyEvade::handle_search(float dt) {
    if (scan_for_target()) {
        if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning,
                   Display,
                   TEXT("Found target: %s"),
                   *ml::get_best_display_name(*target));
        }

        set_state(ETestFlySeekDestroyEvadeState::chasing);
        return handle_chase(dt);
    }

    if (within_radius(destination, config.search.acceptance_radius)) {
        set_new_search_destination();
    }

    move_to_location(dt, destination);
}
void ATestFlySeekDestroyEvade::set_new_search_destination() {
    auto* box{config.search.search_volume->get_box()};
    auto const box_pos{box->GetComponentLocation()};
    auto const box_extent{box->GetScaledBoxExtent()};

    while (within_radius(destination, config.search.min_distance_to_new_point)) {
        destination = UKismetMathLibrary::RandomPointInBoundingBox(box_pos, box_extent);
    }
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
        return false;
    }

    return true;
}

void ATestFlySeekDestroyEvade::handle_chase(float dt) {
    if (!target.IsValid()) {
        log_target_not_valid();
        set_state(ETestFlySeekDestroyEvadeState::searching);
        return;
    }

    auto const target_pos{target->GetActorLocation()};

    if (within_radius(target_pos, config.chase.acceptance_radius)) {
        set_state(ETestFlySeekDestroyEvadeState::attacking);
        return handle_attack(dt);
    }

    move_to_location(dt, target_pos);
}

void ATestFlySeekDestroyEvade::handle_attack(float dt) {
    if (!target.IsValid()) {
        log_target_not_valid();
        set_state(ETestFlySeekDestroyEvadeState::searching);
        return;
    }

    auto const pos{GetActorLocation()};
    auto const target_pos{target->GetActorLocation()};

    if (within_radius(target_pos, config.attack.reposition_radius)) {
        if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning, Display, TEXT("Too close to target, repositioning."));
        }

        set_state(ETestFlySeekDestroyEvadeState::attack_repositioning);
        handle_attack_repositioning(dt);
    }

    // Work out the attack position

    move_to_location(dt, target_pos);

    auto const cos_threshold{
        FMath::Cos(FMath::DegreesToRadians(config.attack.max_fire_angle_degrees))};
    auto const to_enemy{(target_pos - pos).GetSafeNormal()};
    auto const forward{GetActorForwardVector()};
    auto const can_fire{FVector::DotProduct(forward, to_enemy) >= cos_threshold};

    if (can_fire) {
        fire_laser();
    }
}
void ATestFlySeekDestroyEvade::fire_laser() {
    if (!burst.can_fire()) {
        return;
    }

    if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
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
}

void ATestFlySeekDestroyEvade::handle_attack_repositioning(float dt) {
    if (!target.IsValid()) {
        log_target_not_valid();
        set_state(ETestFlySeekDestroyEvadeState::searching);
        return;
    }

    if (within_radius(destination, config.attack_reposition.acceptance_radius)) {
        set_state(ETestFlySeekDestroyEvadeState::chasing);
    }

    move_to_location(dt, destination);
}
void ATestFlySeekDestroyEvade::set_reposition_destination() {
    set_new_search_destination();
}

void ATestFlySeekDestroyEvade::handle_evade(float dt) {}

// Movement
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
auto ATestFlySeekDestroyEvade::within_radius(FVector const& point, float const r) const -> bool {
    return FVector::DistSquared(point, GetActorLocation()) <= (r * r);
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

            drawer.draw_arrow(pos, target->GetActorLocation());
            drawer.draw_sphere(pos, config.chase.acceptance_radius);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            auto const& drawer{config.attack.visuals_config.debug_drawer};

            drawer.draw_arrow(pos, target->GetActorLocation());

            break;
        }
        case ETestFlySeekDestroyEvadeState::attack_repositioning: {
            auto const& drawer{config.attack_reposition.visuals_config.debug_drawer};

            drawer.draw_sphere(destination, config.search.acceptance_radius);
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
    if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning, Display, TEXT("Target is no longer valid."));
    }
}

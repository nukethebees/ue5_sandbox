#include "TestFlySeekDestroyEvade.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestVolume.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Kismet/KismetMathLibrary.h>

ATestFlySeekDestroyEvade::ATestFlySeekDestroyEvade()
    : fire_point{CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"))} {}

void ATestFlySeekDestroyEvade::OnConstruction(FTransform const& t) {
    Super::OnConstruction(t);

    transition_to_state();
}
void ATestFlySeekDestroyEvade::BeginPlay() {
    Super::BeginPlay();

    if (!config.search.search_volume) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("search_volume is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    set_state(ETestFlySeekDestroyEvadeState::searching);
}
void ATestFlySeekDestroyEvade::Tick(float dt) {
    Super::Tick(dt);

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
        case ETestFlySeekDestroyEvadeState::evading: {
            handle_evade(dt);
            break;
        }
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
    state = new_state;
    transition_to_state();
}
void ATestFlySeekDestroyEvade::transition_to_state() {
    switch (state) {
        case ETestFlySeekDestroyEvadeState::searching: {
            set_movement(config.search.movement);
            destination = GetActorLocation();
            target.Reset();
            break;
        }
        case ETestFlySeekDestroyEvadeState::chasing: {
            set_movement(config.chase.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::attacking: {
            set_movement(config.attack.movement);
            break;
        }
        case ETestFlySeekDestroyEvadeState::evading: {
            set_movement(config.evade.movement);
            break;
        }
    }
}

void ATestFlySeekDestroyEvade::handle_search(float dt) {
    if (scan_for_target()) {
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

void ATestFlySeekDestroyEvade::handle_chase(float dt) {}
void ATestFlySeekDestroyEvade::handle_attack(float dt) {}
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

#include "TestFlySearchChase.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/misc/learning/TestVolume.h"
#include "Sandbox/utilities/actor_utils.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"

#include <Components/BoxComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>
#include <Kismet/KismetMathLibrary.h>

ATestFlySearchChase::ATestFlySearchChase() {}

void ATestFlySearchChase::Tick(float dt) {
    Super::Tick(dt);

    switch (state) {
        case ETestFlySearchChaseState::searching: {
            handle_search(dt);
            break;
        }
        case ETestFlySearchChaseState::chasing: {
            handle_chase(dt);
            break;
        }
    }

    if (show_debug_shapes) {
        draw_debug_shapes();
    }
}

void ATestFlySearchChase::BeginPlay() {
    Super::BeginPlay();

    if (!search_volume) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("search_volume is nullptr"));
        SetActorTickEnabled(false);
    }

    draw_config.world = GetWorld();
    set_state(ETestFlySearchChaseState::searching);

    draw_config.cone_angle_half_width_deg = vision.half_angle_deg;
    draw_config.cone_angle_half_height_deg = vision.half_angle_deg;
}

// Movement
auto ATestFlySearchChase::within_radius(FVector const& point, float const r) const -> bool {
    return FVector::DistSquared(point, GetActorLocation()) <= (r * r);
}
auto ATestFlySearchChase::at_destination() const -> bool {
    return within_radius(search_destination, acceptance_radius);
}

// State
void ATestFlySearchChase::set_state(ETestFlySearchChaseState new_state) {
    state = new_state;

    switch (state) {
        case ETestFlySearchChaseState::searching: {
            reset_search_destination();
            material_state.config = search_material_config;
            break;
        }
        case ETestFlySearchChaseState::chasing: {
            material_state.config = chase_material_config;
            break;
        }
    }

    material_state.update_instance();
}

// Search
void ATestFlySearchChase::handle_search(float dt) {
    if (scan_for_target()) {
        set_state(ETestFlySearchChaseState::chasing);
        return handle_chase(dt);
    }

    if (at_destination()) {
        if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
            UE_LOG(LogSandboxLearning,
                   Display,
                   TEXT("(%s) At destination. Finding new destination."),
                   *ml::get_best_display_name(*this));
        }

        set_new_search_destination();
    }

    move_to_location(dt, search_destination);
}
void ATestFlySearchChase::set_new_search_destination() {
    auto* box{search_volume->get_box()};
    auto const box_pos{box->GetComponentLocation()};
    auto const box_extent{box->GetScaledBoxExtent()};

    while (within_radius(search_destination, min_distance_to_new_point)) {
        search_destination = UKismetMathLibrary::RandomPointInBoundingBox(box_pos, box_extent);
    }

    if (log_config.can_log(EActorLoggingVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT("(%s) New destination: %s"),
               *ml::get_best_display_name(*this),
               *search_destination.ToCompactString());
    }
}
void ATestFlySearchChase::reset_search_destination() {
    search_destination = GetActorLocation();
}
auto ATestFlySearchChase::scan_for_target() -> bool {
    FVector origin;
    FVector extent;

    GetActorBounds(false, origin, extent);

    auto hits{ml::find_actors_within_cone(
        *this, vision.radius, extent.Z * 2.0, vision.half_angle_rad(), ThisClass::StaticClass())};

    chase_target = ml::get_centre_actor_in_fov(*this, hits);
    if (!chase_target.IsValid()) {
        return false;
    }

    return true;
}

// Chase
void ATestFlySearchChase::handle_chase(float dt) {
    if (!chase_target.IsValid()) {
        set_state(ETestFlySearchChaseState::searching);
        return;
    }

    move_to_location(dt, chase_target->GetActorLocation());
}
void ATestFlySearchChase::move_to_location(float dt, FVector const& location) {
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

void ATestFlySearchChase::draw_debug_shapes() {
    TRY_INIT_PTR(world, GetWorld());
    auto const pos{GetActorLocation()};

    draw_config.draw_sphere(pos, acceptance_radius);

    switch (state) {
        case ETestFlySearchChaseState::searching: {
            draw_config.draw_line(pos, search_destination);
            draw_config.draw_cone(pos, GetActorForwardVector(), vision.radius);

            draw_config.draw_sphere(search_destination, destination_sphere_radius);

            break;
        }
        case ETestFlySearchChaseState::chasing: {
            draw_config.draw_line(pos, chase_target->GetActorLocation());
            break;
        }
    }
}

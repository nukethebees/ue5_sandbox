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
}

// Movement
auto ATestFlySearchChase::at_destination() const -> bool {
    return (search_destination - GetActorLocation()).SizeSquared() <=
           (acceptance_radius * acceptance_radius);
}

// State
void ATestFlySearchChase::set_state(ETestFlySearchChaseState new_state) {
    state = ETestFlySearchChaseState::searching;

    switch (state) {
        case ETestFlySearchChaseState::searching: {
            reset_search_destination();
            material_state.config = search_material_config;
            break;
        }
        case ETestFlySearchChaseState::chasing: {
            chase_target = nullptr;
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
        set_new_search_destination();
    }

    move_to_location(dt, search_destination);
}
void ATestFlySearchChase::set_new_search_destination() {
    auto* box{search_volume->get_box()};
    auto const point{UKismetMathLibrary::RandomPointInBoundingBox(box->GetComponentLocation(),
                                                                  box->GetScaledBoxExtent())};

    search_destination = point;
}
void ATestFlySearchChase::reset_search_destination() {
    search_destination = GetActorLocation();
}
auto ATestFlySearchChase::scan_for_target() -> bool {
    FVector origin;
    FVector extent;

    GetActorBounds(false, origin, extent);

    auto hits{ml::find_actors_within_cone(
        *this, vision.radius, extent.Z * 2.0, vision.half_angle_rad(), {})};

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

    switch (state) {
        case ETestFlySearchChaseState::searching: {
            draw_config.draw_line(pos, search_destination);
            draw_config.draw_cone(pos, (search_destination - pos).GetSafeNormal(), vision.radius);
            break;
        }
        case ETestFlySearchChaseState::chasing: {
            draw_config.draw_line(pos, chase_target->GetActorLocation());
            break;
        }
    }
}

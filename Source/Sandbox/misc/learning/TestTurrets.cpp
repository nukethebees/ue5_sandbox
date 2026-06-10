#include "TestTurrets.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"
#include "TestLasers.h"
#include "TestTurretsConfig.h"

#include <SandboxCore/actor_components.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/interpolation.h>
#include <SandboxCore/invoke.h>
#include <SandboxCore/rotation.h>
#include <SandboxCore/uobject_utils.h>

#include "CollisionShape.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace {
void move_common(auto const& from_state, auto& to_state, int32 const i) {
    to_state.body_meshes.Add(from_state.body_meshes[i]);
    to_state.cannon_meshes.Add(from_state.cannon_meshes[i]);
    to_state.yaw_pivots.Add(from_state.yaw_pivots[i]);
    to_state.pitch_pivots.Add(from_state.pitch_pivots[i]);
    to_state.collision_shapes.Add(from_state.collision_shapes[i]);

    to_state.location_xs.Add(from_state.location_xs[i]);
    to_state.location_ys.Add(from_state.location_ys[i]);
    to_state.location_zs.Add(from_state.location_zs[i]);

    to_state.yaw_radians.Add(from_state.yaw_radians[i]);

    to_state.healths.Add(from_state.healths[i]);
};

void remove_elem(auto& arr, int32 const i) {
    arr.RemoveAtSwap(i, 1, EAllowShrinking::No);
}

void remove_from_all(int32 const i, auto&... arrays) {
    (remove_elem(arrays, i), ...);
}

auto remove_common(auto& array, int32 const i) {
    remove_elem(array.body_meshes, i);
    remove_elem(array.cannon_meshes, i);
    remove_elem(array.yaw_pivots, i);
    remove_elem(array.pitch_pivots, i);
    remove_elem(array.collision_shapes, i);

    remove_elem(array.location_xs, i);
    remove_elem(array.location_ys, i);
    remove_elem(array.location_zs, i);

    remove_elem(array.yaw_radians, i);

    remove_elem(array.healths, i);
}
}

auto FTestTurretsSearchData::num_turrets() const -> int32 {
    return yaw_radians.Num();
}
auto FTestTurretsSearchData::num_turrets_to_move() const -> int32 {
    return to_attack.Num();
}

void FTestTurretsSearchData::add_uninitialised(int32 const n) {
    ml::invoke_on_all([n](auto& array) { array.AddDefaulted(n); },
                      body_meshes,
                      cannon_meshes,
                      yaw_pivots,
                      pitch_pivots,
                      collision_shapes);

    ml::invoke_on_all([n](auto& array) { array.AddUninitialized(n); },
                      location_xs,
                      location_ys,
                      location_zs,
                      yaw_radians,
                      healths);
}

void FTestTurretsSearchData::reset() {
    ml::invoke_on_all([](auto& x) { ml::destroy_components_array(x); },
                      body_meshes,
                      cannon_meshes,
                      yaw_pivots,
                      pitch_pivots,
                      collision_shapes);

    ml::reset(
        location_xs, location_ys, location_zs, yaw_radians, to_attack, attack_targets, healths);
}
bool FTestTurretsSearchData::array_sizes_consistent() const {
    return ml::all_num_equal(body_meshes,
                             cannon_meshes,
                             yaw_pivots,
                             pitch_pivots,
                             collision_shapes,
                             location_xs,
                             location_ys,
                             location_zs,
                             yaw_radians,
                             healths);
}
void FTestTurretsSearchData::rotate_by(float* yaw_radians,
                                       int32 const n,
                                       float const dt,
                                       float const r) {
    auto const delta{dt * r};

    for (int32 i{0}; i < n; ++i) {
        yaw_radians[i] += delta;
        if (yaw_radians[i] >= 360.f) {
            yaw_radians[i] -= 360.f;
        }
    }
}
void FTestTurretsSearchData::rotate_by(float const dt, float const r) {
    rotate_by(yaw_radians.GetData(), yaw_radians.Num(), dt, r);
}

auto FTestTurretsAttackData::num_turrets() const -> int32 {
    return yaw_radians.Num();
}
auto FTestTurretsAttackData::num_turrets_to_move() const -> int32 {
    return to_search.Num();
}
bool FTestTurretsAttackData::array_sizes_consistent() const {
    return ml::all_num_equal(body_meshes,
                             cannon_meshes,
                             yaw_pivots,
                             pitch_pivots,
                             collision_shapes,
                             location_xs,
                             location_ys,
                             location_zs,
                             target_location_xs,
                             target_location_ys,
                             target_location_zs,
                             pitch_radians,
                             yaw_radians,
                             target_pitch_radians,
                             target_yaw_radians,
                             targets,
                             healths,
                             firing_cooldowns);
}
void FTestTurretsAttackData::reset() {
    ml::invoke_on_all([](auto& x) { ml::destroy_components_array(x); },
                      body_meshes,
                      cannon_meshes,
                      yaw_pivots,
                      pitch_pivots,
                      collision_shapes);

    ml::reset(yaw_radians, yaw_radians, target_pitch_radians, target_yaw_radians);
}

ATestTurrets::ATestTurrets()
    :
#if WITH_EDITOR
    fire_point_marker{CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point_marker"))}
#endif
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

#if WITH_EDITOR
    fire_point_marker->SetupAttachment(RootComponent);
#endif

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

// Actor lifecycle
void ATestTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

#if WITH_EDITOR
    fire_point_marker->bIsEditorOnly = true;
    fire_point_marker->SetHiddenInGame(true);
#endif
}
void ATestTurrets::BeginPlay() {
    Super::BeginPlay();

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(laser_actor),
        SANDBOX_NAMED_UOBJECT_PTR(turret_config),
    });
    if (!turret_config->is_ready()) {
        UE_LOG(LogSandboxLearning, Fatal, TEXT("turret_config is not ready."));
    }

#if WITH_EDITOR
    {
        auto* world{GetWorld()};
        turret_config->searching_debug_drawer.world = world;
        turret_config->attacking_debug_drawer.world = world;
    }
#endif

    update_locations_from_components();
    check_arrays_synced();
}
void ATestTurrets::Tick(float dt) {
    Super::Tick(dt);

    // Handles may be invalid so we want to prune them quickly
    handle_transitions_to_searching();

    update_target_rotations();
    integrate_rotations(dt);
    apply_rotations_to_components();

    tick_attacking_cooldowns(dt);
    fire_when_aligned();

    perform_search();
    handle_transitions_to_attacking();

    log_config.tick(dt);

#if WITH_EDITOR
    if (debug_shapes_enabled) {
        draw_debug_shapes();
    }
#endif
}

auto ATestTurrets::num_turrets() const noexcept -> int32 {
    return searching.num_turrets() + attacking.num_turrets();
}

void ATestTurrets::update_locations_from_components() {
    auto const n_searching{searching.num_turrets()};

    for (int32 i{0}; i < n_searching; ++i) {
        auto const loc{searching.collision_shapes[i]->GetComponentLocation()};

        searching.location_xs[i] = loc.X;
        searching.location_ys[i] = loc.Y;
        searching.location_zs[i] = loc.Z;
    }
}

// Searching
bool ATestTurrets::is_enemy(AActor const& actor) const {
    for (auto const& target_class : valid_target_classes) {
        if (actor.IsA(target_class)) {
            return true;
        }
    }

    return false;
}
void ATestTurrets::perform_search() {
    auto const n{searching.num_turrets()};

    TRY_INIT_PTR(world, GetWorld());

    TArray<FOverlapResult> overlaps;

    FCollisionObjectQueryParams object_query_params{};
    FCollisionQueryParams query_params{};
    query_params.AddIgnoredActor(this);

    auto const detection_radius{turret_config->detection_radius};

    auto const shape{FCollisionShape::MakeSphere(detection_radius)};

    FHitResult los_hit;

    for (int32 i{0}; i < n; ++i) {
        overlaps.Reset();

        FVector const loc{
            searching.location_xs[i],
            searching.location_ys[i],
            searching.location_zs[i],
        };

        if (!world->OverlapMultiByObjectType(
                overlaps, loc, FQuat::Identity, object_query_params, shape, query_params)) {
            continue;
        }

        for (auto& overlap : overlaps) {
            auto* actor{overlap.GetActor()};

            if ((!IsValid(actor)) || (actor == this)) {
                continue;
            }

            if (!is_enemy(*actor)) {
                continue;
            }

            auto const end{actor->GetActorLocation()};
            los_hit.Reset();

            bool const los_blocked{
                world->LineTraceSingleByChannel(los_hit, loc, end, ECC_Visibility, query_params)};

            if ((!los_blocked) || (los_hit.GetActor() == actor)) {
                searching.to_attack.Add(i);
                searching.attack_targets.Add(actor);
                break;
            }
        }
    }
}

// Tracking
void ATestTurrets::update_target_rotations() {
    auto const n{attacking.num_turrets()};

    // Update locations
    for (int32 i{0}; i < n; ++i) {
        auto const target_location{attacking.targets[i]->GetActorLocation()};
        attacking.target_location_xs[i] = target_location.X;
        attacking.target_location_ys[i] = target_location.Y;
        attacking.target_location_zs[i] = target_location.Z;
    }

    // Update yaws
    using T = float;
    for (int32 i{0}; i < n; ++i) {
        ml::compute_desired_yaws_radians(TConstArrayView<T>{attacking.location_xs},
                                         TConstArrayView<T>{attacking.location_ys},
                                         TConstArrayView<T>{attacking.target_location_xs},
                                         TConstArrayView<T>{attacking.target_location_ys},
                                         TArrayView<T>{attacking.target_yaw_radians});
    }
}
void ATestTurrets::integrate_rotations(float const dt) {
    ml::rotate_towards_1d_radians_normalised_in_place(
        TArrayView<float>{attacking.yaw_radians},
        TConstArrayView<float>{attacking.target_yaw_radians},
        FMath::DegreesToRadians(yaw_rotation_speed_degrees),
        dt);
}
void ATestTurrets::apply_rotations_to_components() {}

// Attacking
void ATestTurrets::fire_when_aligned() {
    auto const n{attacking.num_turrets()};

    auto const fire_threshold{FMath::DegreesToRadians(turret_config->valid_attack_angle_degrees)};
    auto const cooldown{turret_config->attack_cooldown};
    auto const laser_class{turret_config->laser_class};
    auto const laser_damage{turret_config->laser_damage};
    auto const laser_offset{turret_config->fire_point_offset};

    TRY_INIT_PTR(world, GetWorld());
    TArray<FTransform> laser_transforms;

    for (int32 i{0}; i < n; ++i) {
        if (attacking.firing_cooldowns[i] > 0.f) {
            continue;
        }

        if (attacking.yaw_radians[i] <= fire_threshold) {
            continue;
        }

        FVector const base_location{
            attacking.location_xs[i],
            attacking.location_ys[i],
            attacking.location_zs[i],
        };
        FVector const fire_point{base_location + laser_offset.GetLocation()};

        FVector const attack_location{
            attacking.target_location_xs[i],
            attacking.target_location_ys[i],
            attacking.target_location_zs[i],
        };

        auto const attack_direction_vec{(attack_location - fire_point).GetSafeNormal()};
        auto const attack_direction_rot{attack_direction_vec.Rotation()};

        FTransform const laser_transform{
            attack_direction_rot,
            fire_point,
            FVector::OneVector,
        };

        laser_transforms.Add(laser_transform);
        attacking.firing_cooldowns[i] = cooldown;
    }

    laser_actor->spawn_lasers(laser_transforms);
}
void ATestTurrets::tick_attacking_cooldowns(float const dt) {
    auto const n{attacking.num_turrets()};

    for (int32 i{0}; i < n; ++i) {
        attacking.firing_cooldowns[i] -= dt;
    }
}

// Transitions
void ATestTurrets::handle_transitions_to_searching() {
    auto const n_attacking{attacking.num_turrets()};

    for (int32 i{0}; i < n_attacking; ++i) {
        if (!attacking.targets[i].IsValid()) {
            attacking.to_search.Add(i);
        }
    }

    auto const n_to_search{attacking.num_turrets_to_move()};

    for (int32 move_i{0}; move_i < n_to_search; ++move_i) {
        auto const turret_i{attacking.to_search[move_i]};

        move_common(attacking, searching, turret_i);
    }

    for (int32 move_i{n_to_search - 1}; move_i >= 0; --move_i) {
        auto const turret_i{attacking.to_search[move_i]};

        remove_common(attacking, turret_i);
        remove_from_all(turret_i,
                        attacking.pitch_radians,
                        attacking.target_location_xs,
                        attacking.target_location_ys,
                        attacking.target_location_zs,
                        attacking.target_pitch_radians,
                        attacking.target_yaw_radians,
                        attacking.targets);
    }

    attacking.to_search.Reset();

    check_arrays_synced();
}
void ATestTurrets::handle_transitions_to_attacking() {
    auto const n_to_attack{searching.num_turrets_to_move()};

    constexpr auto add_defaulted{
        [](int32 const n, auto&... arrays) { (arrays.AddDefaulted(n), ...); }};

    add_defaulted(n_to_attack,
                  attacking.pitch_radians,
                  attacking.target_location_xs,
                  attacking.target_location_ys,
                  attacking.target_location_zs,
                  attacking.target_pitch_radians,
                  attacking.target_yaw_radians,
                  attacking.firing_cooldowns);

    for (int32 move_i{0}; move_i < n_to_attack; ++move_i) {
        auto const turret_i{searching.to_attack[move_i]};

        move_common(searching, attacking, turret_i);

        attacking.targets.Add(searching.attack_targets[move_i]);
    }

    for (int32 move_i{n_to_attack - 1}; move_i >= 0; --move_i) {
        auto const turret_i{searching.to_attack[move_i]};

        remove_common(searching, turret_i);
    }
    searching.to_attack.Reset();
    searching.attack_targets.Reset();
}

// Spawning
void ATestTurrets::create_turrets(int32 const n) {
    if (!turret_config) {
        UE_LOG(LogSandboxLearning, Error, TEXT("Cannot create turrets. Turret config is nullptr."));
        return;
    }

    auto const cur_num_turrets{num_turrets()};
    auto const cur_num_searching_turrets{searching.num_turrets()};

    searching.add_uninitialised(n);

    auto const hp{turret_config->max_health};
    FName name;

    for (int32 i{0}; i < n; ++i) {
        auto const turret_index{cur_num_turrets + i};
        auto const search_index{cur_num_searching_turrets + i};

        name = MakeUniqueObjectName(this, UCapsuleComponent::StaticClass(), TEXT("Turret"));
        auto* collision{NewObject<UCapsuleComponent>(this, name)};

        name = MakeUniqueObjectName(this, USceneComponent::StaticClass(), TEXT("YawPivot"));
        auto* yaw_pivot{NewObject<USceneComponent>(this, name)};

        name = MakeUniqueObjectName(this, USceneComponent::StaticClass(), TEXT("PitchPivot"));
        auto* pitch_pivot{NewObject<USceneComponent>(this, name)};

        name = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("BodyMesh"));
        auto* body{NewObject<UStaticMeshComponent>(this, name)};

        name = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), TEXT("CannonMesh"));
        auto* cannon{NewObject<UStaticMeshComponent>(this, name)};

        collision->SetupAttachment(RootComponent);
        yaw_pivot->SetupAttachment(collision);
        body->SetupAttachment(yaw_pivot);
        pitch_pivot->SetupAttachment(yaw_pivot);
        cannon->SetupAttachment(pitch_pivot);

        ml::register_components(collision, yaw_pivot, pitch_pivot, body, cannon);

        AddInstanceComponent(collision);
        // We use the first turret for positioning components
        // but we don't want the panel getting filled up
        if (turret_index == 0) {
            AddInstanceComponent(yaw_pivot);
            AddInstanceComponent(pitch_pivot);
            AddInstanceComponent(body);
            AddInstanceComponent(cannon);
        }

        searching.collision_shapes[search_index] = collision;
        searching.yaw_pivots[search_index] = yaw_pivot;
        searching.pitch_pivots[search_index] = pitch_pivot;
        searching.body_meshes[search_index] = body;
        searching.cannon_meshes[search_index] = cannon;

        body->SetStaticMesh(turret_config->body_mesh);
        configure_collision(*body);
        cannon->SetStaticMesh(turret_config->cannon_mesh);
        configure_collision(*cannon);

        collision->SetRelativeTransform(turret_config->collision_offset);
        yaw_pivot->SetRelativeTransform(turret_config->yaw_pivot_offset);
        pitch_pivot->SetRelativeTransform(turret_config->pitch_pivot_offset);
        body->SetRelativeTransform(turret_config->body_offset);
        cannon->SetRelativeTransform(turret_config->cannon_offset);

        collision->SetCapsuleSize(turret_config->collision_radius,
                                  turret_config->collision_half_height);

        auto const loc{turret_config->collision_offset.GetLocation()};
        searching.location_xs[i] = loc.X;
        searching.location_ys[i] = loc.Y;
        searching.location_zs[i] = loc.Z;

        searching.healths[i] = hp;

        searching.yaw_radians[i] = 0.f;
    }

    check_arrays_synced();
}
void ATestTurrets::configure_collision(UStaticMeshComponent& sm) {
    sm.SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ATestTurrets::clear_all_turrets() {
    searching.reset();
    attacking.reset();
}
#if WITH_EDITOR
void ATestTurrets::capture_turret_layout(int32 const i) {
    if (!turret_config) {
        UE_LOG(LogSandboxLearning, Error, TEXT("Cannot capture layout. Turret config is nullptr."));
        return;
    }

    auto const n_turrets{num_turrets()};
    if (i >= n_turrets) {
        UE_LOG(LogSandboxLearning,
               Error,
               TEXT("Cannot capture layout. Index %d is out of range (%d turrets)"),
               i,
               n_turrets);
        return;
    }

    auto& conf{*turret_config};

    FScopedTransaction const transaction{
        NSLOCTEXT("SandboxLearning", "CaptureTurretLayout", "Capture Turret Layout")};

    conf.Modify();

    auto const root_transform{searching.collision_shapes[i]->GetRelativeTransform()};

    conf.collision_offset = root_transform;
    conf.body_offset = searching.body_meshes[i]->GetRelativeTransform();
    conf.cannon_offset = searching.cannon_meshes[i]->GetRelativeTransform();
    conf.yaw_pivot_offset = searching.yaw_pivots[i]->GetRelativeTransform();
    conf.pitch_pivot_offset = searching.pitch_pivots[i]->GetRelativeTransform();

    conf.fire_point_offset =
        fire_point_marker->GetComponentTransform().GetRelativeTransform(root_transform);

    conf.collision_radius = searching.collision_shapes[i]->GetUnscaledCapsuleRadius();
    conf.collision_half_height = searching.collision_shapes[i]->GetUnscaledCapsuleHalfHeight();

    conf.MarkPackageDirty();
}
void ATestTurrets::create_turrets_button() {
    create_turrets(num_turrets_to_create);
}
void ATestTurrets::capture_turret_0_layout_button() {
    capture_turret_layout(0);
}
#endif

// Debugging
void ATestTurrets::check_arrays_synced() const {
    check(searching.array_sizes_consistent());
    check(attacking.array_sizes_consistent());
}

#if WITH_EDITOR
// Debugging
void ATestTurrets::draw_debug_shapes() {
    if (log_config.can_tick_log(EActorLogVerbosity::Verbose)) {
        UE_LOG(LogSandboxLearning, Verbose, TEXT("Drawing debug shapes"));
    }

    draw_searching_debug_shapes();
    draw_attacking_debug_shapes();
}
void ATestTurrets::draw_searching_debug_shapes() {
    auto const n{searching.num_turrets()};
    auto const& drawer{turret_config->searching_debug_drawer};
    auto const offset{turret_config->debug_sphere_offset};

    auto const search_radius{turret_config->detection_radius};

    for (int32 i{0}; i < n; i++) {
        FVector const loc{
            searching.location_xs[i] + offset.X,
            searching.location_ys[i] + offset.Y,
            searching.location_zs[i] + offset.Z,
        };

        drawer.draw_sphere(loc);
        drawer.draw_sphere(loc, search_radius);

        FRotator const rotation{
            0.0,
            searching.yaw_radians[i],
            0.0,
        };

        drawer.draw_line(loc, rotation);
    }
}
void ATestTurrets::draw_attacking_debug_shapes() {
    auto const n{attacking.num_turrets()};
    auto const& drawer{turret_config->attacking_debug_drawer};
    auto const offset{turret_config->debug_sphere_offset};

    for (int32 i{0}; i < n; i++) {
        FVector const loc{
            attacking.location_xs[i] + offset.X,
            attacking.location_ys[i] + offset.Y,
            attacking.location_zs[i] + offset.Z,
        };

        drawer.draw_sphere(loc);

        auto const target{attacking.targets[i]};

        if (target.IsValid()) {
            drawer.draw_line(loc, target->GetActorLocation());
        } else {
            if (log_config.can_tick_log(EActorLogVerbosity::Basic)) {
                UE_LOG(LogSandboxLearning, Warning, TEXT("Target is invalid"));
            }
        }

        FRotator const rotation{
            FMath::RadiansToDegrees(attacking.pitch_radians[i]),
            FMath::RadiansToDegrees(attacking.yaw_radians[i]),
            FMath::RadiansToDegrees(0.0),
        };

        drawer.draw_line(loc, rotation);
    }
}
#endif

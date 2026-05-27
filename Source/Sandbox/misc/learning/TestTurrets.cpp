#include "TestTurrets.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/macros/null_checks.hpp"
#include "Sandbox/utilities/vision_maths.h"
#include "TestTurretsConfig.h"

#include <SandboxCore/Public/actor_components.h>
#include <SandboxCore/Public/interpolation.h>
#include <SandboxCore/Public/rotation.h>

#include "CollisionShape.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

auto FTestTurretsSearchData::num_turrets() const -> int32 {
    return yaw_degrees.Num();
}
auto FTestTurretsSearchData::num_turrets_to_move() const -> int32 {
    return to_attack.Num();
}

void FTestTurretsSearchData::add_uninitialised(int32 const n) {
    body_meshes.AddDefaulted(n);
    cannon_meshes.AddDefaulted(n);
    yaw_pivots.AddDefaulted(n);
    pitch_pivots.AddDefaulted(n);
    collision_shapes.AddDefaulted(n);

    location_xs.AddUninitialized(n);
    location_ys.AddUninitialized(n);
    location_zs.AddUninitialized(n);

    yaw_degrees.AddUninitialized(n);

    healths.AddUninitialized(n);
}
void FTestTurretsSearchData::reset() {
    ml::destroy_components_array(body_meshes);
    ml::destroy_components_array(cannon_meshes);
    ml::destroy_components_array(yaw_pivots);
    ml::destroy_components_array(pitch_pivots);
    ml::destroy_components_array(collision_shapes);

    location_xs.Reset();
    location_ys.Reset();
    location_zs.Reset();

    yaw_degrees.Reset();

    to_attack.Reset();
    attack_targets.Reset();

    healths.Reset();
}
void FTestTurretsSearchData::rotate_by(float* yaw_degrees,
                                       int32 const n,
                                       float const dt,
                                       float const r) {
    auto const delta{dt * r};

    for (int32 i{0}; i < n; ++i) {
        yaw_degrees[i] += delta;
        if (yaw_degrees[i] >= 360.f) {
            yaw_degrees[i] -= 360.f;
        }
    }
}
void FTestTurretsSearchData::rotate_by(float const dt, float const r) {
    rotate_by(yaw_degrees.GetData(), yaw_degrees.Num(), dt, r);
}

auto FTestTurretsAttackData::num_turrets() const -> int32 {
    return yaw_degrees.Num();
}
void FTestTurretsAttackData::reset() {
    ml::destroy_components_array(body_meshes);
    ml::destroy_components_array(cannon_meshes);
    ml::destroy_components_array(yaw_pivots);
    ml::destroy_components_array(pitch_pivots);
    ml::destroy_components_array(collision_shapes);

    yaw_degrees.Reset();
    yaw_degrees.Reset();
    target_pitch_degrees.Reset();
    target_yaw_degrees.Reset();
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

void ATestTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

#if WITH_EDITOR
    fire_point_marker->bIsEditorOnly = true;
    fire_point_marker->SetHiddenInGame(true);
#endif
}
void ATestTurrets::BeginPlay() {
    Super::BeginPlay();

    if (!turret_config) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("turret_config is nullptr"));
        SetActorTickEnabled(false);
        return;
    }

    if (!turret_config->is_ready()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("turret_config is not ready."));
        SetActorTickEnabled(false);
        return;
    }

#if WITH_EDITOR
    {
        auto* world{GetWorld()};
        turret_config->searching_debug_drawer.world = world;
        turret_config->attacking_debug_drawer.world = world;
    }
#endif

    update_locations_from_components();
}
void ATestTurrets::Tick(float dt) {
    Super::Tick(dt);

    update_target_rotations();
    integrate_rotations(dt);
    apply_rotations_to_components();

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

void ATestTurrets::update_target_rotations() {
    auto const n{num_turrets()};
}
void ATestTurrets::integrate_rotations(float const dt) {
    ml::rotate_towards_1d_degrees_normalised_inplace(
        TArrayView<float>{attacking.pitch_degrees},
        TConstArrayView<float>{attacking.target_pitch_degrees},
        pitch_rotation_speed_degrees,
        dt);

    ml::rotate_towards_1d_degrees_normalised_inplace(
        TArrayView<float>{attacking.yaw_degrees},
        TConstArrayView<float>{attacking.target_yaw_degrees},
        yaw_rotation_speed_degrees,
        dt);
}
void ATestTurrets::apply_rotations_to_components() {}

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

            auto const is_enemy{true};
            if (!is_enemy) {
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
void ATestTurrets::change_turret_state() {
    auto const n_to_attack{searching.num_turrets_to_move()};

    constexpr auto move_common{[](auto const& from_array, auto& to_array, int32 const i) {
        to_array.body_meshes.Add(from_array.body_meshes[i]);
        to_array.cannon_meshes.Add(from_array.cannon_meshes[i]);
        to_array.yaw_pivots.Add(from_array.yaw_pivots[i]);
        to_array.pitch_pivots.Add(from_array.pitch_pivots[i]);
        to_array.collision_shapes.Add(from_array.collision_shapes[i]);

        to_array.location_xs.Add(from_array.location_xs[i]);
        to_array.location_ys.Add(from_array.location_ys[i]);
        to_array.location_zs.Add(from_array.location_zs[i]);

        to_array.yaw_degrees.Add(from_array.yaw_degrees[i]);

        to_array.healths.Add(from_array.healths[i]);
    }};

    constexpr auto remove_elem{
        [](auto& arr, int32 const i) { arr.RemoveAtSwap(i, 1, EAllowShrinking::No); }};

    constexpr auto remove_common{[](auto& array, int32 const i) {
        remove_elem(array.body_meshes, i);
        remove_elem(array.cannon_meshes, i);
        remove_elem(array.yaw_pivots, i);
        remove_elem(array.pitch_pivots, i);
        remove_elem(array.collision_shapes, i);

        remove_elem(array.location_xs, i);
        remove_elem(array.location_ys, i);
        remove_elem(array.location_zs, i);

        remove_elem(array.yaw_degrees, i);

        remove_elem(array.healths, i);
    }};

    for (int32 move_i{0}; move_i < n_to_attack; ++move_i) {
        auto const turret_i{searching.to_attack[move_i]};

        move_common(searching, attacking, turret_i);

        attacking.target_pitch_degrees.AddDefaulted();
        attacking.targets.Add(searching.attack_targets[turret_i]);
    }

    // Remove old elements
    for (int32 move_i{n_to_attack - 1}; move_i >= 0; --move_i) {
        auto const turret_i{searching.to_attack[move_i]};

        remove_common(searching, move_i);
    }
    searching.to_attack.Reset();
    searching.attack_targets.Reset();

    auto const n_to_search{0};
    for (int32 move_i{0}; move_i < n_to_search; ++move_i) {
        auto const turret_i{attacking.to_search[move_i]};

        move_common(searching, attacking, turret_i);
    }

    for (int32 move_i{n_to_attack - 1}; move_i >= 0; --move_i) {
        auto const turret_i{attacking.to_search[move_i]};

        remove_common(attacking, move_i);

        remove_elem(attacking.pitch_degrees, move_i);
        remove_elem(attacking.target_pitch_degrees, move_i);
        remove_elem(attacking.target_yaw_degrees, move_i);

        remove_elem(attacking.targets, move_i);
    }
    attacking.to_search.Reset();
}

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

        collision->RegisterComponent();
        yaw_pivot->RegisterComponent();
        pitch_pivot->RegisterComponent();
        body->RegisterComponent();
        cannon->RegisterComponent();

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

        searching.yaw_degrees[i] = 0.f;
    }
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

// Debugging
void ATestTurrets::draw_debug_shapes() {
    if (log_config.can_tick_log(EActorLoggingVerbosity::Verbose)) {
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
            searching.yaw_degrees[i],
            0.0,
        };

        drawer.draw_line(loc, rotation);
    }
}
void ATestTurrets::draw_attacking_debug_shapes() {}
#endif

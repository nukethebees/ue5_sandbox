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
    body_meshes.AddUninitialized(n);
    cannon_meshes.AddUninitialized(n);
    yaw_pivots.AddUninitialized(n);
    pitch_pivots.AddUninitialized(n);
    collision_shapes.AddUninitialized(n);

    location_xs.AddUninitialized(n);
    location_ys.AddUninitialized(n);
    location_zs.AddUninitialized(n);

    yaw_degrees.AddUninitialized(n);
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
}
void ATestTurrets::Tick(float dt) {
    Super::Tick(dt);

    update_target_rotations();
    integrate_rotations(dt);
    apply_rotations_to_components();
}

auto ATestTurrets::num_turrets() const noexcept -> int32 {
    return searching.num_turrets() + attacking.num_turrets();
}

void ATestTurrets::update_target_rotations() {
    auto const n{num_turrets()};
}
void ATestTurrets::integrate_rotations(float const dt) {
    searching.rotate_by(dt, yaw_rotation_speed_degrees);

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
    auto const shape{FCollisionShape::MakeSphere(detection_radius)};

    for (int32 i{0}; i < n; ++i) {
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

            if ((actor == nullptr) || (actor == this)) {
                continue;
            }

            auto const is_enemy{true};
            if (!is_enemy) {
                continue;
            }

            auto const is_in_los{true};
            if (is_in_los) {
                searching.to_attack.Add(i);
                searching.attack_targets.Add(actor);
                break;
            }
        }

        overlaps.Reset();
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

    auto const num_components{num_turrets()};
    auto const new_total{num_components + n};

    searching.add_uninitialised(n);

    auto const hp{turret_config->max_health};

    for (int32 i{0}; i < n; ++i) {
        auto const component_index{num_components + i};

        auto* collision{NewObject<UCapsuleComponent>(
            this, FName{*FString::Printf(TEXT("Turret_%d"), component_index)})};
        auto* yaw_pivot{NewObject<USceneComponent>(
            this, FName{*FString::Printf(TEXT("YawPivot_%d"), component_index)})};
        auto* pitch_pivot{NewObject<USceneComponent>(
            this, FName{*FString::Printf(TEXT("PitchPivot_%d"), component_index)})};
        auto* body{NewObject<UStaticMeshComponent>(
            this, FName{*FString::Printf(TEXT("BodyMesh_%d"), component_index)})};
        auto* cannon{NewObject<UStaticMeshComponent>(
            this, FName{*FString::Printf(TEXT("CannonMesh_%d"), component_index)})};

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

        searching.collision_shapes[component_index] = collision;
        searching.yaw_pivots[component_index] = yaw_pivot;
        searching.pitch_pivots[component_index] = pitch_pivot;
        searching.body_meshes[component_index] = body;
        searching.cannon_meshes[component_index] = cannon;

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

#if WITH_EDITOR
    FScopedTransaction const transaction{
        NSLOCTEXT("SandboxLearning", "CaptureTurretLayout", "Capture Turret Layout")};

    conf.Modify();
#endif

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

#if WITH_EDITOR
    conf.MarkPackageDirty();
#endif
}
void ATestTurrets::create_turrets_button() {
    create_turrets(num_turrets_to_create);
}
void ATestTurrets::capture_turret_0_layout_button() {
    capture_turret_layout(0);
}
#endif

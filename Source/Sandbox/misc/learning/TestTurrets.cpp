#include "TestTurrets.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestTurretsConfig.h"

#include <SandboxCore/Public/interpolation.h>
#include <SandboxCore/Public/rotation.h>

#include "CollisionShape.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

namespace {
template <typename T>
void destroy_components(TArray<T>& components) {
    for (auto& component : components) {
        if (!IsValid(component)) {
            continue;
        }

        component->Modify();
        component->DestroyComponent();
    }

    components.Reset();
};
}

auto FTestTurretsSearchData::num_turrets() const -> int32 {
    return yaw_degrees.Num();
}
void FTestTurretsSearchData::reset() {
    yaw_degrees.Reset();
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

void ATestTurrets::create_turrets(int32 const n) {
    if (!turret_config) {
        UE_LOG(LogSandboxLearning, Error, TEXT("Cannot create turrets. Turret config is nullptr."));
        return;
    }

    auto const num_components{num_turrets()};
    auto const new_total{num_components + n};

    collision_shapes.Reserve(new_total);
    yaw_pivots.Reserve(new_total);
    pitch_pivots.Reserve(new_total);
    body_meshes.Reserve(new_total);
    cannon_meshes.Reserve(new_total);

    searching.yaw_degrees.AddDefaulted(n);

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

        collision_shapes.Emplace(collision);
        yaw_pivots.Emplace(yaw_pivot);
        pitch_pivots.Emplace(pitch_pivot);
        body_meshes.Emplace(body);
        cannon_meshes.Emplace(cannon);

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
    }
}
void ATestTurrets::configure_collision(UStaticMeshComponent& sm) {
    sm.SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ATestTurrets::clear_all_turrets() {
    destroy_components(collision_shapes);
    destroy_components(yaw_pivots);
    destroy_components(pitch_pivots);
    destroy_components(body_meshes);
    destroy_components(cannon_meshes);

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

    auto const root_transform{collision_shapes[i]->GetRelativeTransform()};

    conf.collision_offset = root_transform;
    conf.body_offset = body_meshes[i]->GetRelativeTransform();
    conf.cannon_offset = cannon_meshes[i]->GetRelativeTransform();
    conf.yaw_pivot_offset = yaw_pivots[i]->GetRelativeTransform();
    conf.pitch_pivot_offset = pitch_pivots[i]->GetRelativeTransform();

    conf.fire_point_offset =
        fire_point_marker->GetComponentTransform().GetRelativeTransform(root_transform);

    conf.collision_radius = collision_shapes[i]->GetUnscaledCapsuleRadius();
    conf.collision_half_height = collision_shapes[i]->GetUnscaledCapsuleHalfHeight();

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

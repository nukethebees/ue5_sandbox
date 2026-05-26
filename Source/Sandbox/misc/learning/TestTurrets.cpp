#include "TestTurrets.h"

#include "Sandbox/combat/weapons/ShipLaser.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestTurretsConfig.h"

#include <SandboxCore/Public/interpolation.h>
#include <SandboxCore/Public/rotation.h>

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

ATestTurrets::ATestTurrets() {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ATestTurrets::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
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
    return ai_states.Num();
}

void ATestTurrets::update_target_rotations() {
    auto const n{num_turrets()};

    for (int32 i{0}; i < n; ++i) {
        auto const ai_state{ai_states[i]};

        switch (ai_state) {
            case ETestTurretState::scanning: {
                break;
            }
            case ETestTurretState::attacking: {
                break;
            }
        }
    }
}
void ATestTurrets::integrate_rotations(float const dt) {
    auto const n{num_turrets()};

    ml::rotate_towards_1d_degrees_normalised_inplace(TArrayView<float>{pitch_degrees},
                                                     TConstArrayView<float>{target_pitch_degrees},
                                                     pitch_rotation_speed_degrees,
                                                     dt);

    ml::rotate_towards_1d_degrees_normalised_inplace(TArrayView<float>{yaw_degrees},
                                                     TConstArrayView<float>{target_yaw_degrees},
                                                     yaw_rotation_speed_degrees,
                                                     dt);
}
void ATestTurrets::apply_rotations_to_components() {
    auto const n{num_turrets()};

    for (int32 i{0}; i < n; ++i) {
        body_meshes[i]->SetRelativeRotation(FRotator{0.f, yaw_degrees[i], 0.f});
        cannon_meshes[i]->SetRelativeRotation(FRotator{pitch_degrees[i], 0.f, 0.f});
    }
}

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

    ai_states.AddDefaulted(n);

    pitch_degrees.AddDefaulted(n);
    yaw_degrees.AddDefaulted(n);
    target_pitch_degrees.AddDefaulted(n);
    target_yaw_degrees.AddDefaulted(n);

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
        cannon->SetStaticMesh(turret_config->cannon_mesh);

        collision->SetRelativeTransform(turret_config->collision_offset);
        yaw_pivot->SetRelativeTransform(turret_config->yaw_pivot_offset);
        pitch_pivot->SetRelativeTransform(turret_config->pitch_pivot_offset);
        body->SetRelativeTransform(turret_config->body_offset);
        cannon->SetRelativeTransform(turret_config->cannon_offset);
    }
}
void ATestTurrets::clear_all_turrets() {
    destroy_components(collision_shapes);
    destroy_components(yaw_pivots);
    destroy_components(pitch_pivots);
    destroy_components(body_meshes);
    destroy_components(cannon_meshes);

    ai_states.Reset();

    pitch_degrees.Reset();
    yaw_degrees.Reset();
    target_pitch_degrees.Reset();
    target_yaw_degrees.Reset();
}

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

    conf.collision_offset = collision_shapes[i]->GetRelativeTransform();
    conf.body_offset = body_meshes[i]->GetRelativeTransform();
    conf.cannon_offset = cannon_meshes[i]->GetRelativeTransform();
    conf.yaw_pivot_offset = yaw_pivots[i]->GetRelativeTransform();
    conf.pitch_pivot_offset = pitch_pivots[i]->GetRelativeTransform();

#if WITH_EDITOR
    conf.MarkPackageDirty();
#endif
}

#if WITH_EDITOR
void ATestTurrets::create_turrets_button() {
    create_turrets(num_turrets_to_create);
}
void ATestTurrets::capture_turret_0_layout_button() {
    capture_turret_layout(0);
}
#endif

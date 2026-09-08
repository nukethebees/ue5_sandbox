#include "SpaceGame/defences/spinners/TestTubeSpinnerProxy.h"

#include "SpaceGame/entities/TestProxyActorFunctions.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>
#if WITH_EDITOR
#include <ScopedTransaction.h>
#endif

ATestTubeSpinnerProxy::ATestTubeSpinnerProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
}

#if WITH_EDITOR
void ATestTubeSpinnerProxy::add_fire_points(int32 const n) {
    for (int32 i{0}; i < n; i++) {
        auto const name{
            MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("FirePoint"))};

        auto* fire_point{
            ml::create_attached_instance_component<UArrowComponent>(*this, name, *mesh)};
        if (!fire_point) {
            UE_LOG(LogSandboxLearning,
                   Warning,
                   TEXT("ATestTubeSpinnerProxy::add_fire_point: new fire_point is nullptr."));
            return;
        }

        fire_points.Add(fire_point);
    }
}
void ATestTubeSpinnerProxy::add_fire_point() {
    add_fire_points(1);
}

void ATestTubeSpinnerProxy::remove_fire_points(int32 const n) {
    ml::destroy_components_at_array_end(fire_points, n);
}
void ATestTubeSpinnerProxy::remove_all_fire_points() {
    ml::destroy_components_array(fire_points);
}
void ATestTubeSpinnerProxy::remove_fire_point() {
    remove_fire_points(1);
}

void ATestTubeSpinnerProxy::position_fire_points() {
    face_fire_points_away_from_mesh();
}
void ATestTubeSpinnerProxy::face_fire_points_away_from_mesh() {
    auto const mesh_location{mesh->GetComponentLocation()};

    for (auto fire_point : fire_points) {
        auto const fp_location{fire_point->GetComponentLocation()};
        auto const away_from_mesh{(fp_location - mesh_location).GetSafeNormal()};

        fire_point->SetWorldRotation({0.f, away_from_mesh.Rotation().Yaw, 0.f});
    }
}
void ATestTubeSpinnerProxy::set_random_active_fire_point() {
    if (!actor_config) {
        return;
    }

    auto const n_fire_points{actor_config->fire_point_offsets.Num()};
    if (n_fire_points < 1) {
        return;
    }

    initial_active_fire_point = FMath::RandRange(0, n_fire_points);
}
void ATestTubeSpinnerProxy::set_random_active_fire_point_to_all_instances() {
    ml::for_each_instance(*this, [](ThisClass& x) { x.set_random_active_fire_point(); });
}

void ATestTubeSpinnerProxy::apply_asset_configuration() {
    auto const* const config{ml::resolve_proxy_level_config(*this)};
    if (!IsValid(config)) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("Cannot apply spinner configuration: requires exactly one orchestrator with a "
                    "level config."));
        return;
    }
    FScopedTransaction const transaction{
        NSLOCTEXT("SpinnerProxy", "Apply", "Apply spinner configuration")};
    Modify();
    actor_config = &config->tube_spinners;
    mesh->Modify();

    mesh->SetStaticMesh(actor_config->mesh);
    auto const n_fire_points{actor_config->fire_point_offsets.Num()};
    auto const old_count{fire_points.Num()};
    for (int32 i{n_fire_points}; i < old_count; ++i) {
        if (auto* const point{fire_points[i].Get()}; IsValid(point)) {
            point->SetFlags(RF_Transactional);
            point->Modify();
            RemoveInstanceComponent(point);
            point->DestroyComponent();
        }
    }
    fire_points.SetNum(n_fire_points);

    for (int32 i{0}; i < n_fire_points; ++i) {
        if (!IsValid(fire_points[i])) {
            auto const name{
                MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("FirePoint"))};
            auto* const point{NewObject<UArrowComponent>(this, name, RF_Transactional)};
            point->SetupAttachment(mesh);
            AddInstanceComponent(point);
            point->RegisterComponent();
            fire_points[i] = point;
        }
        fire_points[i]->SetFlags(RF_Transactional);
        fire_points[i]->Modify();
        fire_points[i]->SetRelativeTransform(actor_config->fire_point_offsets[i]);
    }
    MarkPackageDirty();
}
void ATestTubeSpinnerProxy::apply_asset_configuration_to_all_instances() {
    FScopedTransaction const transaction{
        NSLOCTEXT("SpinnerProxy", "ApplyAll", "Apply configuration to all spinners")};
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}

#endif

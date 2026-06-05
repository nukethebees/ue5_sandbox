#include "TestTubeSpinnerProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestTubeSpinnersConfig.h"

#include <SandboxCore/actor_components.h>
#include <SandboxCore/actor_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>

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

void ATestTubeSpinnerProxy::save_configuration_to_asset() {
    if (!actor_config) {
        return;
    }

    actor_config->Modify();

    actor_config->fire_point_offsets.Reset();
    for (auto fire_point : fire_points) {
        actor_config->fire_point_offsets.Add(fire_point->GetRelativeTransform());
    }

    actor_config->MarkPackageDirty();
}

void ATestTubeSpinnerProxy::apply_asset_configuration() {
    if (!actor_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestTubeSpinnerProxy::apply_asset_configuration: actor_config is nullptr."));
        return;
    }

    mesh->SetStaticMesh(actor_config->mesh);
    remove_all_fire_points();

    auto const n_fire_points{actor_config->fire_point_offsets.Num()};
    add_fire_points(n_fire_points);

    for (int32 i{0}; i < n_fire_points; ++i) {
        fire_points[i]->SetRelativeTransform(actor_config->fire_point_offsets[i]);
    }
}
void ATestTubeSpinnerProxy::apply_asset_configuration_to_all_instances() {
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}

#endif

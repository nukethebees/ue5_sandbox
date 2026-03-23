#include "Sandbox/players/MothershipBoss.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/DamageableShip.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

#include <type_traits>

#include "Sandbox/utilities/macros/null_checks.hpp"

AMothershipBoss::AMothershipBoss()
    : mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("main_ship"))} {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh_component->SetupAttachment(RootComponent);

    add_n_components<n_hatches>(hatch_meshes, TEXT("hatch"));
    add_n_components<n_hatches>(hatch_lights, TEXT("hatch_light"));
    add_n_components<n_search_lights>(search_light_meshes, TEXT("search_light"));
    add_n_components<n_point_lights>(point_lights, TEXT("point_light"));
}

template <int32 N, typename T>
void AMothershipBoss::add_n_components(TArray<T*>& components, FString const& name_base) {
    for (int32 i{0}; i < N; i++) {
        auto const name{FString::Printf(TEXT("%s_%d"), *name_base, i)};
        auto* new_component{CreateDefaultSubobject<T>(*name)};
        if (!new_component) {
            WARN_IS_FALSE(LogSandboxActor, new_component);
        }
        new_component->SetupAttachment(mesh_component);
        components.Add(new_component);
    }
}

void AMothershipBoss::Tick(float dt) {
    Super::Tick(dt);

    auto const delta_rotation{rotation_speed * dt};
    SetActorRotation(GetActorRotation() + delta_rotation);

    auto const delta_hatch_light_rotation{hatch_light_rotation_speed * dt};
    for (auto* light : hatch_lights) {
        light->AddRelativeRotation(delta_hatch_light_rotation);
    }

    auto const delta_pos{dt * movement_speed};
    auto const loc{GetActorLocation()};
    if (target) {
        auto const tgt_loc{target->GetActorLocation()};
        auto const tgt_loc_adjusted{tgt_loc + target_offset};
        auto const dist{FVector::Dist(tgt_loc_adjusted, loc)};

        if (dist > target_arrived_threshold) {
            auto const dir{(tgt_loc_adjusted - loc).GetSafeNormal()};
            SetActorLocation(loc + dir * delta_pos);
        }
    }
}
void AMothershipBoss::BeginPlay() {
    Super::BeginPlay();
}
void AMothershipBoss::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    UE_LOG(LogSandboxActor, Log, TEXT("AMothershipBoss::OnConstruction"));

    RETURN_IF_NULLPTR(mesh_component);
    TRY_INIT_PTR(mesh, mesh_component->GetStaticMesh());

    attach_components<n_hatches>(*mesh_component, *mesh, hatch_meshes, TEXT("Hatch"));
    attach_components<n_hatches>(*mesh_component, *mesh, hatch_lights, TEXT("HatchLight"));
    attach_components<n_search_lights>(
        *mesh_component, *mesh, search_light_meshes, TEXT("SearchLight"));
    attach_components<n_point_lights>(*mesh_component, *mesh, point_lights, TEXT("ShipLight"));

    set_meshes(hatch_mesh, hatch_meshes);
    set_meshes(search_light_mesh, search_light_meshes);

    RETURN_IF_NULLPTR(search_light_mesh_material);
    for (auto* c : search_light_meshes) {
        c->SetMaterial(0, search_light_mesh_material);
    }

    config_point_lights();
    config_hatch_lights();
}
template <int32 N, typename T>
void AMothershipBoss::attach_components(UStaticMeshComponent& parent,
                                        UStaticMesh& parent_mesh,
                                        TArray<T*>& components,
                                        FString const& socket_base) {
    auto const n_components{components.Num()};
    if (n_components != N) {
        UE_LOG(LogSandboxActor, Warning, TEXT("Incorrect number of components"));
        return;
    }

    auto const rules{FAttachmentTransformRules::SnapToTargetNotIncludingScale};

    for (int32 i{0}; i < N; i++) {
        auto const socket_name{FString::Printf(TEXT("%s_%d"), *socket_base, i)};
        auto* socket{parent_mesh.FindSocket(*socket_name)};
        CONTINUE_IF_NULLPTR(socket);
        auto* component{components[i]};
        CONTINUE_IF_NULLPTR(component);
        component->AttachToComponent(&parent, rules, *socket_name);
    }
}
void AMothershipBoss::set_meshes(UStaticMesh* mesh, TArray<UStaticMeshComponent*>& components) {
    RETURN_IF_NULLPTR(mesh);
    for (auto* c : components) {
        c->SetStaticMesh(mesh);
    }
}
void AMothershipBoss::config_point_lights() {
    for (auto* pl : point_lights) {
        CONTINUE_IF_NULLPTR(pl);
        pl->SetLightColor(point_light_settings.colour);
        pl->SetLightBrightness(point_light_settings.brightness);
        pl->SetAttenuationRadius(point_light_settings.attenuation_radius);
        pl->SetSourceRadius(point_light_settings.source_radius);
    }
}
void AMothershipBoss::config_hatch_lights() {
    for (auto* pl : hatch_lights) {
        CONTINUE_IF_NULLPTR(pl);
        pl->SetLightColor(hatch_light_settings.colour);
        pl->SetLightBrightness(hatch_light_settings.brightness);
        pl->SetSourceRadius(hatch_light_settings.source_radius);
        pl->SetAttenuationRadius(hatch_light_settings.attenuation_radius);
        pl->SetInnerConeAngle(hatch_light_settings.inner_cone_angle);
        pl->SetOuterConeAngle(hatch_light_settings.outer_cone_angle);
    }
}

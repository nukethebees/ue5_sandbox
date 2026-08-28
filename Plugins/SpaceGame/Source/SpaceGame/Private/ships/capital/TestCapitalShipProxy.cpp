#include "SpaceGame/ships/capital/TestCapitalShipProxy.h"

#include "SpaceGame/entities/TestProxyActorFunctions.h"
#include "SpaceGame/entities/TestTeamVisualData.h"
#include "SpaceGame/ships/capital/TestCapitalShips.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>

ATestCapitalShipProxy::ATestCapitalShipProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATestCapitalShipProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ml::set_proxy_actor_name(*this, TEXT("CapitalShip"), team);

    if (!actor_config) {
        return;
    }

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(mesh),
        })}) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShipProxy::OnConstruction Uobject ptrs are invalid: %s"),
               *msg);
        return;
    }

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        })}) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShipProxy::OnConstruction Uobject ptrs are invalid: %s"),
               *msg);
        return;
    }

    if (!ml::is_valid(team)) {
        UE_LOG(LogSandbox, Warning, TEXT("ATestCapitalShipProxy::OnConstruction Team is invalid"));
    }

    mesh->SetStaticMesh(actor_config->mesh);

    actor_config->team_visual_data->ensure_all_team_colours_exist();
    auto const colour_cache{
        UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};

    auto const colour{colour_cache[team]};
    mesh->SetCustomPrimitiveDataVector3f(0, FVector3f{colour});
}

#if WITH_EDITOR
void ATestCapitalShipProxy::apply_asset_configuration() {
    if (!actor_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::apply_asset_configuration: actor_config is nullptr."));
        return;
    }

    ml::destroy_components_array(fighter_spawn_slots);
    fighter_spawn_slots.Reserve(actor_config->fighter_spawn_slots);

    for (int32 i{0}; i < actor_config->fighter_spawn_slots; ++i) {
        auto const name{
            MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("SpawnPoint"))};
        auto* spawn_point{NewObject<UArrowComponent>(this, name)};

        spawn_point->SetupAttachment(mesh);
        spawn_point->RegisterComponent();
        AddInstanceComponent(spawn_point);
        fighter_spawn_slots.Add(spawn_point);

        spawn_point->SetRelativeTransform(actor_config->fighter_spawn_slots_relative_transforms[i]);
        spawn_point->SetArrowSize(actor_config->proxy_arrow_size);
    }
}
void ATestCapitalShipProxy::apply_asset_configuration_to_all_instances() {
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}
#endif

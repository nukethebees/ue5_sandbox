#include "TestCapitalShipProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShips.h"
#include "TestCapitalShipsConfig.h"
#include "TestTeamVisualData.h"

#include <SandboxCore/actor_components.h>
#include <SandboxCore/actor_utils.h>
#include <SandboxCore/collision_settings.h>
#include <SandboxCore/uobject_utils.h>

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

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config),
            SANDBOX_NAMED_UOBJECT_PTR(mesh),
        })};
        !msg.IsEmpty()) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShipProxy::OnConstruction Uobject ptrs are invalid: %s"),
               *msg);
        return;
    }

    if (auto const msg{ml::report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        })};
        !msg.IsEmpty()) {
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
void ATestCapitalShipProxy::save_configuration_to_asset() {
    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning,
            Warning,
            TEXT("ATestCapitalShipProxy::save_configuration_to_asset: actor_config is nullptr."));
        return;
    }

    actor_config->Modify();

    if (actor_config->fighter_spawn_slots < 1) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: fighter_spawn_slots < 1 "
                    "(%d)."),
               actor_config->fighter_spawn_slots);
        return;
    }

    auto& transforms{actor_config->fighter_spawn_slots_relative_transforms};

    transforms.Reset();
    transforms.Reserve(actor_config->fighter_spawn_slots);
    for (auto const slot : fighter_spawn_slots) {
        transforms.Add(slot->GetRelativeTransform());
        actor_config->proxy_arrow_size = slot->ArrowSize;
    }

    actor_config->MarkPackageDirty();
}

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

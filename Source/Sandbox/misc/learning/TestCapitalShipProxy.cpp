#include "TestCapitalShipProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestCapitalShips.h"
#include "TestCapitalShipsConfig.h"

#include <SandboxCore/actor_components.h>
#include <SandboxCore/collision_settings.h>

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>

ATestCapitalShipProxy::ATestCapitalShipProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , collision_box{CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATestCapitalShipProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!batch_actor) {
        return;
    }

    if (!ship_config) {
        return;
    }

    mesh->SetStaticMesh(ship_config->mesh);
}
void ATestCapitalShipProxy::BeginPlay() {
    Super::BeginPlay();

    if (target_ship == this) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShipProxy: target_ship is this."));
        return;
    }

    if (!batch_actor) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("ATestCapitalShipProxy: batch_actor is nullptr."));
        return;
    }
}

#if WITH_EDITOR
void ATestCapitalShipProxy::save_configuration_to_asset() {
    if (!ship_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: ship_config is nullptr."));
        return;
    }

    ship_config->Modify();

    if (ship_config->fighter_spawn_slots < 1) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: fighter_spawn_slots < 1 "
                    "(%d)."),
               ship_config->fighter_spawn_slots);
        return;
    }

    auto& transforms{ship_config->fighter_spawn_slots_relative_transforms};

    transforms.Reset();
    transforms.Reserve(ship_config->fighter_spawn_slots);
    for (auto const slot : fighter_spawn_slots) {
        transforms.Add(slot->GetRelativeTransform());
    }

    if (!collision_box) {
        UE_LOG(
            LogSandboxLearning,
            Warning,
            TEXT("ATestCapitalShipProxy::save_configuration_to_asset: collision_box is nullptr."));
        return;
    }

    ship_config->collision_box_extent = collision_box->GetUnscaledBoxExtent();
    ship_config->collision_settings = ml::copy_collision_settings(*collision_box);

    ship_config->MarkPackageDirty();
}

void ATestCapitalShipProxy::apply_asset_configuration() {
    if (!ship_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::apply_asset_configuration: ship_config is nullptr."));
        return;
    }

    ml::destroy_components_array(fighter_spawn_slots);
    fighter_spawn_slots.Reserve(ship_config->fighter_spawn_slots);

    for (int32 i{0}; i < ship_config->fighter_spawn_slots; ++i) {
        auto const name{
            MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("SpawnPoint"))};
        auto* spawn_point{NewObject<UArrowComponent>(this, name)};

        spawn_point->SetupAttachment(mesh);
        spawn_point->RegisterComponent();
        AddInstanceComponent(spawn_point);
        fighter_spawn_slots.Add(spawn_point);

        spawn_point->SetRelativeTransform(ship_config->fighter_spawn_slots_relative_transforms[i]);
    }

    collision_box->SetBoxExtent(ship_config->collision_box_extent);
    ml::apply_collision_settings(*collision_box, ship_config->collision_settings);
}
void ATestCapitalShipProxy::apply_asset_configuration_to_all_instances() {
    auto* world{GetWorld()};
    if (!world) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::apply_asset_configuration_to_all_instances: world is "
                    "nullptr."));
        return;
    }

    for (auto it{TActorIterator<ATestCapitalShipProxy>(world)}; it; ++it) {
        if (!IsValid(*it)) {
            continue;
        }

        (*it)->apply_asset_configuration();
    }
}
#endif

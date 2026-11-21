#include "SpaceGame/ships/capital/TestCapitalShipProxy.h"

#include "SpaceGame/entities/TestProxyActorFunctions.h"
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxCoreEngine/uobject_utils.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>

#if WITH_EDITOR
#include <ioj/sim/levels/fighter_launch_column.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>

#include <ScopedTransaction.h>
#endif

namespace ioj {
ATestCapitalShipProxy::ATestCapitalShipProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITOR
    PrimaryActorTick.bCanEverTick = true;
#endif
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATestCapitalShipProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    set_proxy_actor_name(*this, TEXT("CapitalShip"), team);

    refresh_mesh();
}

void ATestCapitalShipProxy::refresh_mesh() {
    auto const* config{s7_level_config::load_canonical()};
    if (!IsValid(config)) {
        UE_LOG(LogSandbox, Warning, TEXT("Capital preview requires the shared level asset."));
        return;
    }
    auto const* actor_config{&config->capital_ships};

    if (auto const msg{report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(mesh),
        })}) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShipProxy::OnConstruction Uobject ptrs are invalid: %s"),
               *msg);
        return;
    }

    if (auto const msg{report_invalid_uobject_ptrs({
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->mesh),
            SANDBOX_NAMED_UOBJECT_PTR(actor_config->team_visual_data),
        })}) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("ATestCapitalShipProxy::OnConstruction Uobject ptrs are invalid: %s"),
               *msg);
        return;
    }

    if (!is_valid(team)) {
        UE_LOG(LogSandbox, Warning, TEXT("ATestCapitalShipProxy::OnConstruction Team is invalid"));
    }

    mesh->SetStaticMesh(actor_config->mesh);

    auto const colour{actor_config->team_visual_data->get_colour(team)};
    mesh->SetCustomPrimitiveDataVector3f(0, FVector3f{colour});
}

#if WITH_EDITOR
void ATestCapitalShipProxy::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    auto const* const world{GetWorld()};
    bool const editor_world{IsValid(world) && world->WorldType == EWorldType::Editor};
    SetActorTickEnabled(editor_world);
    if (editor_world && HasAnyFlags(RF_WasLoaded)) {
        refresh_mesh();
    }
    for (auto const slot : fighter_spawn_slots) {
        if (IsValid(slot)) {
            slot->SetVisibility(false);
        }
    }
}

void ATestCapitalShipProxy::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    auto const* const world{GetWorld()};
    if (IsValid(world) && world->WorldType == EWorldType::Editor && IsSelected()) {
        update_spawn_configuration_status();
        if (show_fighter_spawn_preview) {
            draw_fighter_spawn_preview();
        }
    }
}

void ATestCapitalShipProxy::update_spawn_configuration_status() {
    auto* const config{s7_level_config::load_canonical()};
    if (!IsValid(config)) {
        spawn_configuration_status = TEXT("Shared level asset unavailable.");
        return;
    }
    spawn_configuration_status =
        FString::Printf(TEXT("Launches %d fighters above the carrier."),
                        config->capital_ships.fighter_spawn_slots_relative_transforms.Num());
}

void ATestCapitalShipProxy::draw_fighter_spawn_preview(float lifetime) {
    auto* const world{GetWorld()};
    auto const* const config{s7_level_config::load_canonical()};
    auto const report_error{[this](FString const& message) {
        if (spawn_preview_error != message) {
            UE_LOG(LogSandbox,
                   Warning,
                   TEXT("Fighter spawn preview for %s: %s"),
                   *GetPathName(),
                   *message);
            spawn_preview_error = message;
        }
    }};
    if (!IsValid(config) || !IsValid(config->capital_ships.mesh) ||
        !IsValid(config->fighters.mesh)) {
        report_error(TEXT("A level configuration with capital and fighter meshes is required."));
        return;
    }

    FLevelCollisionHost::EntityMeshes meshes{};
    meshes[sim::EntityType::CapitalShip] = config->capital_ships.mesh;
    meshes[sim::EntityType::Fighter] = config->fighters.mesh;
    auto const local_bounds{FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!local_bounds) {
        report_error(local_bounds.error().format());
        return;
    }
    spawn_preview_error.Reset();
    auto const pivot{GetActorLocation()};
    auto const rotation{GetActorRotation()};
    auto const column{sim::fighter_launch_column(*local_bounds,
                                                 Vector3f{pivot},
                                                 Rotator3f{static_cast<float>(rotation.Pitch),
                                                           static_cast<float>(rotation.Yaw),
                                                           static_cast<float>(rotation.Roll)},
                                                 preview_clearance_)};
    auto const direction{FRotator{0., column.heading.yaw, 0.}.Vector()};
    auto const count{config->capital_ships.fighter_spawn_slots_relative_transforms.Num()};
    for (int32 i{}; i < count; ++i) {
        auto const location{column.position(i)};
        auto const bounds{sim::collision::make_entity_world_bounds(
            *local_bounds, sim::EntityType::Fighter, location, to_quaternion(column.heading))};
        auto const position{FVector{to_unreal(location)}};
        DrawDebugBox(world,
                     FVector{to_unreal((bounds.min + bounds.max) * 0.5f)},
                     FVector{to_unreal((bounds.max - bounds.min) * 0.5f)},
                     FColor::Cyan,
                     false,
                     lifetime);
        DrawDebugDirectionalArrow(
            world, position, position + direction * 600.f, 120.f, FColor::Cyan, false, lifetime);
    }
}

void ATestCapitalShipProxy::diagnose_fighter_spawn_points() {
    draw_fighter_spawn_preview(30.f);
}

void ATestCapitalShipProxy::save_configuration_to_asset() {
    auto* const config{s7_level_config::load_canonical()};
    if (!IsValid(config)) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: no writable level "
                    "configuration asset is assigned."));
        return;
    }

    auto const slot_count{fighter_spawn_slots.Num()};
    auto const* actor_config{&config->capital_ships};
    if (slot_count != actor_config->fighter_spawn_slots_relative_transforms.Num()) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: expected %d fighter "
                    "spawn slots but found %d components."),
               actor_config->fighter_spawn_slots_relative_transforms.Num(),
               slot_count);
        return;
    }
    for (int32 slot_index{}; slot_index < slot_count; ++slot_index) {
        if (!IsValid(fighter_spawn_slots[slot_index])) {
            UE_LOG(LogSandboxLearning,
                   Warning,
                   TEXT("ATestCapitalShipProxy::save_configuration_to_asset: fighter spawn slot "
                        "%d is invalid."),
                   slot_index);
            return;
        }
    }

    FScopedTransaction const transaction{
        NSLOCTEXT("CapitalShipProxy", "SaveSpawnSlots", "Save capital spawn layout")};
    config->Modify();
    auto& capital_config{config->capital_ships};
    capital_config.fighter_spawn_slots_relative_transforms.Reset(slot_count);
    FTransform const spawn_frame{GetActorRotation(), GetActorLocation(), FVector::OneVector};
    for (auto const slot : fighter_spawn_slots) {
        capital_config.fighter_spawn_slots_relative_transforms.Add(
            slot->GetComponentTransform().GetRelativeTransform(spawn_frame));
    }
    config->MarkPackageDirty();
    update_spawn_configuration_status();
}

void ATestCapitalShipProxy::apply_asset_configuration() {
    auto* const config{s7_level_config::load_canonical()};
    if (!IsValid(config)) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("Cannot apply capital configuration: shared level asset unavailable."));
        return;
    }
    FScopedTransaction const transaction{
        NSLOCTEXT("CapitalShipProxy", "ApplySpawnSlots", "Apply capital spawn layout")};
    apply_spawn_configuration(*config);
}

void ATestCapitalShipProxy::apply_spawn_configuration(USpaceGameLevelConfig& config) {
    auto const& capital_config{config.capital_ships};
    Modify();
    auto const slot_count{capital_config.fighter_spawn_slots_relative_transforms.Num()};
    auto const old_count{fighter_spawn_slots.Num()};
    for (int32 i{slot_count}; i < old_count; ++i) {
        auto const slot{fighter_spawn_slots[i]};
        if (IsValid(slot)) {
            slot->Modify();
            RemoveInstanceComponent(slot);
            slot->DestroyComponent();
        }
    }
    fighter_spawn_slots.SetNum(slot_count);

    FTransform const spawn_frame{GetActorRotation(), GetActorLocation(), FVector::OneVector};
    for (int32 i{}; i < slot_count; ++i) {
        auto* spawn_point{fighter_spawn_slots[i].Get()};
        // Preserve authored Blueprint components and their references when the slot exists.
        if (!IsValid(spawn_point)) {
            auto const name{
                MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("SpawnPoint"))};
            spawn_point = NewObject<UArrowComponent>(this, name, RF_Transactional);
            spawn_point->SetupAttachment(RootComponent);
            AddInstanceComponent(spawn_point);
            spawn_point->RegisterComponent();
            fighter_spawn_slots[i] = spawn_point;
        }
        spawn_point->SetFlags(RF_Transactional);
        spawn_point->Modify();

        spawn_point->SetVisibility(false);
        spawn_point->SetWorldTransform(capital_config.fighter_spawn_slots_relative_transforms[i] *
                                       spawn_frame);
        spawn_point->SetArrowSize(preview_arrow_size_);
    }
    MarkPackageDirty();
    update_spawn_configuration_status();
}
void ATestCapitalShipProxy::apply_asset_configuration_to_all_instances() {
    auto* const config{s7_level_config::load_canonical()};
    if (!IsValid(config)) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("Cannot apply capital configuration: shared level asset unavailable."));
        return;
    }
    FScopedTransaction const transaction{
        NSLOCTEXT("CapitalShipProxy", "ApplyAllSpawnSlots", "Apply spawn layout to all capitals")};
    for_each_instance(*this,
                      [config](ThisClass& proxy) { proxy.apply_spawn_configuration(*config); });
}
#endif

} // namespace ioj

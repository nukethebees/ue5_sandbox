#include "SpaceGame/ships/capital/TestCapitalShipProxy.h"

#include "SpaceGame/entities/TestProxyActorFunctions.h"
#include "SpaceGame/entities/TestTeamUtils.h"
#include "SpaceGame/entities/TestTeamVisualData.h"
#include "SpaceGame/simulation/TestBatchOrchestrator.h"
#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/actor_utils.h>
#include <SandboxCoreEngine/collision_settings.h>
#include <SandboxCoreEngine/uobject_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/BoxComponent.h>
#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/StaticMesh.h>
#include <EngineUtils.h>

#if WITH_EDITOR
#include <SpaceGame/simulation/EntityWorldBounds.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/support/mesh.h>
#endif

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

void ATestCapitalShipProxy::set_level_config_asset(
    USpaceGameLevelConfig* const new_config) noexcept {
    level_config_asset = new_config;
    actor_config = IsValid(new_config) ? &new_config->capital_ships : nullptr;
}

void ATestCapitalShipProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ml::set_proxy_actor_name(*this, TEXT("CapitalShip"), team);

    if (IsValid(level_config_asset)) {
        actor_config = &level_config_asset->capital_ships;
    }
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
void ATestCapitalShipProxy::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    auto const* const world{GetWorld()};
    SetActorTickEnabled(IsValid(world) && world->WorldType == EWorldType::Editor);
}

void ATestCapitalShipProxy::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    auto const* const world{GetWorld()};
    if (IsValid(world) && world->WorldType == EWorldType::Editor && IsSelected() &&
        show_fighter_spawn_preview) {
        draw_fighter_spawn_preview();
    }
}

void ATestCapitalShipProxy::draw_fighter_spawn_preview() {
    auto* const world{GetWorld()};
    auto const* const orchestrator{ml::get_first_actor<ATestBatchOrchestrator>(*world)};
    auto const* const config{IsValid(orchestrator) ? orchestrator->get_level_config()
                                                   : level_config_asset.Get()};
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

    ml::ioj::FLevelCollisionHost::EntityMeshes meshes{};
    meshes[ETestEntityType::CapitalShip] = config->capital_ships.mesh;
    auto const local_bounds{ml::ioj::FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!local_bounds) {
        report_error(local_bounds.error().format());
        return;
    }
    auto const slot_count{fighter_spawn_slots.Num()};
    for (int32 i{}; i < slot_count; ++i) {
        if (!IsValid(fighter_spawn_slots[i])) {
            report_error(FString::Printf(TEXT("Spawn arrow %d is missing."), i));
            return;
        }
    }
    spawn_preview_error.Reset();
    // Runtime uses the actor pivot/rotation, not the preview mesh component's relative transform.
    auto const bounds{ml::ioj::make_entity_world_bounds(*local_bounds,
                                                        ml::ioj::FEntityAABBs::capital_ship_index,
                                                        FVector3f{GetActorLocation()},
                                                        FRotator3f{GetActorRotation()})};
    auto const clearance{ml::get_mesh_sphere_bounds(*config->fighters.mesh) +
                         config->fighters.avoidance_clearance_buffer};
    auto const clearance_bounds{bounds.ExpandBy(clearance)};
    DrawDebugBox(
        world, FVector{bounds.GetCenter()}, FVector{bounds.GetExtent()}, FColor::Green, false, 0.f);
    DrawDebugBox(world,
                 FVector{clearance_bounds.GetCenter()},
                 FVector{clearance_bounds.GetExtent()},
                 FColor::Orange,
                 false,
                 0.f);

    auto const minimum_spacing_sq{FMath::Square(clearance * 2.f)};
    for (int32 i{}; i < slot_count; ++i) {
        auto const* const arrow{fighter_spawn_slots[i].Get()};
        auto const position{arrow->GetComponentLocation()};
        auto const blocked{clearance_bounds.IsInsideOrOn(FVector3f{position})};
        bool too_close{};
        for (int32 j{}; j < slot_count; ++j) {
            auto const* const neighbour{fighter_spawn_slots[j].Get()};
            if (i != j && IsValid(neighbour) &&
                FVector::DistSquared(position, neighbour->GetComponentLocation()) <
                    minimum_spacing_sq) {
                too_close = true;
                break;
            }
        }
        auto const colour{blocked || too_close ? FColor::Red : FColor::Cyan};
        DrawDebugSphere(world, position, 150.f, 12, colour, false, 0.f);
        DrawDebugDirectionalArrow(world,
                                  position,
                                  position + arrow->GetForwardVector() * 600.f,
                                  120.f,
                                  colour,
                                  false,
                                  0.f);
    }
}

void ATestCapitalShipProxy::diagnose_fighter_spawn_points() {
    auto* const world{GetWorld()};
    auto* const orchestrator{IsValid(world) ? ml::get_first_actor<ATestBatchOrchestrator>(*world)
                                            : nullptr};
    auto const* const runtime_config{IsValid(orchestrator) ? orchestrator->get_level_config()
                                                           : nullptr};
    if (!IsValid(runtime_config)) {
        UE_LOG(LogSandbox,
               Warning,
               TEXT("[FighterSpawn] No orchestrator level config for %s"),
               *GetPathName());
        return;
    }
    FTransform const spawn_frame{GetActorRotation(), GetActorLocation(), FVector::OneVector};
    UE_LOG(LogSandbox,
           Display,
           TEXT("[FighterSpawn] Proxy=%s runtimeConfig=%s savedOwner=%s actor=%s meshRelative=%s "
                "mesh=%s runtimeMesh=%s arrows=%d (cyan=arrow, green=predicted spawn)"),
           *GetPathName(),
           *runtime_config->GetPathName(),
           *GetPathNameSafe(level_config_asset),
           *GetActorTransform().ToHumanReadableString(),
           IsValid(mesh) ? *mesh->GetRelativeTransform().ToHumanReadableString() : TEXT("missing"),
           IsValid(mesh) ? *GetPathNameSafe(mesh->GetStaticMesh()) : TEXT("missing"),
           *GetPathNameSafe(runtime_config->capital_ships.mesh),
           fighter_spawn_slots.Num());
    auto const& slots{runtime_config->capital_ships.fighter_spawn_slots_relative_transforms};
    auto const count{slots.Num()};
    for (int32 i{}; i < count; ++i) {
        auto const predicted{(slots[i] * spawn_frame).GetLocation()};
        DrawDebugSphere(world, predicted, 150.f, 12, FColor::Green, false, 30.f);
        auto const* const arrow{fighter_spawn_slots.IsValidIndex(i) ? fighter_spawn_slots[i].Get()
                                                                    : nullptr};
        if (!IsValid(arrow)) {
            UE_LOG(LogSandbox,
                   Display,
                   TEXT("[FighterSpawn] Slot=%d predicted=%s arrow=missing"),
                   i,
                   *predicted.ToString());
            continue;
        }
        auto const actual{arrow->GetComponentLocation()};
        DrawDebugSphere(world, actual, 100.f, 12, FColor::Cyan, false, 30.f);
        DrawDebugLine(world, actual, predicted, FColor::Red, false, 30.f);
        UE_LOG(LogSandbox,
               Display,
               TEXT("[FighterSpawn] Slot=%d saved=%s predicted=%s arrow=%s deltaCm=%.2f parent=%s"),
               i,
               *slots[i].ToHumanReadableString(),
               *predicted.ToString(),
               *actual.ToString(),
               FVector::Dist(actual, predicted),
               *GetPathNameSafe(arrow->GetAttachParent()));
    }
}

void ATestCapitalShipProxy::save_configuration_to_asset() {
    if (!IsValid(level_config_asset)) {
        auto* const world{GetWorld()};
        auto* const orchestrator{
            IsValid(world) ? ml::get_first_actor<ATestBatchOrchestrator>(*world) : nullptr};
        if (IsValid(orchestrator)) {
            set_level_config_asset(orchestrator->get_level_config());
        }
    }
    if (!IsValid(level_config_asset) || actor_config != &level_config_asset->capital_ships) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: no writable level "
                    "configuration asset is assigned."));
        return;
    }

    auto const slot_count{fighter_spawn_slots.Num()};
    if (slot_count != actor_config->fighter_spawn_slots) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::save_configuration_to_asset: expected %d fighter "
                    "spawn slots but found %d components."),
               actor_config->fighter_spawn_slots,
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

    level_config_asset->Modify();
    auto& capital_config{level_config_asset->capital_ships};
    capital_config.fighter_spawn_slots_relative_transforms.Reset(slot_count);
    // Simulation spawns from the actor's position and rotation, without actor or mesh scale.
    FTransform const spawn_frame{GetActorRotation(), GetActorLocation(), FVector::OneVector};
    for (auto const slot : fighter_spawn_slots) {
        capital_config.fighter_spawn_slots_relative_transforms.Add(
            slot->GetComponentTransform().GetRelativeTransform(spawn_frame));
        capital_config.proxy_arrow_size = slot->ArrowSize;
    }
    level_config_asset->MarkPackageDirty();
}

void ATestCapitalShipProxy::apply_asset_configuration() {
    if (!actor_config) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::apply_asset_configuration: actor_config is nullptr."));
        return;
    }

    if (actor_config->fighter_spawn_slots < 0 ||
        actor_config->fighter_spawn_slots_relative_transforms.Num() !=
            actor_config->fighter_spawn_slots) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("ATestCapitalShipProxy::apply_asset_configuration: saved spawn slot count "
                    "does not match its transforms; existing arrows were left unchanged."));
        return;
    }

    ml::destroy_components_array(fighter_spawn_slots);
    fighter_spawn_slots.Reserve(actor_config->fighter_spawn_slots);

    FTransform const spawn_frame{GetActorRotation(), GetActorLocation(), FVector::OneVector};
    for (int32 i{0}; i < actor_config->fighter_spawn_slots; ++i) {
        auto const name{
            MakeUniqueObjectName(this, UArrowComponent::StaticClass(), TEXT("SpawnPoint"))};
        auto* spawn_point{NewObject<UArrowComponent>(this, name)};

        spawn_point->SetupAttachment(RootComponent);
        spawn_point->RegisterComponent();
        AddInstanceComponent(spawn_point);
        fighter_spawn_slots.Add(spawn_point);

        spawn_point->SetWorldTransform(actor_config->fighter_spawn_slots_relative_transforms[i] *
                                       spawn_frame);
        spawn_point->SetArrowSize(actor_config->proxy_arrow_size);
    }
}
void ATestCapitalShipProxy::apply_asset_configuration_to_all_instances() {
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}
#endif

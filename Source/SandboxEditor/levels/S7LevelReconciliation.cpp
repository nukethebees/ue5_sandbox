#include "SandboxEditor/levels/S7LevelReconciliation.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <ioj/levels/authoring/editor_support.h>
#include <ioj/levels/authoring/scene_reconciliation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <LevelUtils.h>
#include <ScopedTransaction.h>

namespace ioj::editor {
namespace s7_level_reconciliation_detail {
auto make_changes(ULevel const& level,
                  AS7LevelAuthoringDocument const& document,
                  levels::LevelDefinition const& incoming)
    -> std::expected<levels::authoring::SyncPlan, FString> {
    auto const& definition{incoming.components()};
    auto* const world{GEditor ? GEditor->GetEditorWorldContext().World() : nullptr};
    if (!IsValid(world) || world->IsGameWorld() || level.OwningWorld != world ||
        world->GetCurrentLevel() != &level || document.GetLevel() != &level) {
        return std::unexpected{TEXT("The target is not the current editor level.")};
    }
    if (FLevelUtils::IsLevelLocked(const_cast<ULevel*>(&level))) {
        return std::unexpected{TEXT("The current editor level is locked.")};
    }
    if (!IsValid(document.level_config)) {
        return std::unexpected{TEXT("The authoring document has no level configuration.")};
    }

    // Validate every configured class before any actor is spawned or modified.
    for (auto const& entity : definition.entities) {
        auto* const type{s7_level_actor_class(entity.archetype, *document.level_config)};
        if (!IsValid(type) ||
            type->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient)) {
            return std::unexpected{FString::Printf(TEXT("Entity '%s' cannot be materialised."),
                                                   *ioj::to_fstring(entity.id.value))};
        }
    }
    auto scene{collect_s7_scene(level, document)};
    if (!scene) {
        return std::unexpected{scene.error()};
    }
    return levels::authoring::reconcile_scene(*scene, incoming);
}

auto transform(levels::EntitySpawnDefinition const& entity) -> FTransform {
    return FTransform{FRotator{entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll},
                      FVector{entity.position.x(), entity.position.y(), entity.position.z()}};
}
auto name(levels::EntityId const& id) -> FName {
    return FName{ioj::to_fstring(id.value)};
}
}

auto make_s7_level_sync_plan(ULevel const& level,
                             AS7LevelAuthoringDocument const& document,
                             levels::LevelDefinition definition)
    -> std::expected<FS7LevelSyncPlan, FString> {
    auto support{levels::authoring::check_editor_support(definition)};
    if (!support) {
        return std::unexpected{ioj::to_fstring(levels::format_diagnostics(support.error()))};
    }
    auto changes{s7_level_reconciliation_detail::make_changes(level, document, definition)};
    if (!changes) {
        return std::unexpected{changes.error()};
    }
    return FS7LevelSyncPlan{MoveTemp(*changes), MoveTemp(definition)};
}

auto apply_s7_level_sync_plan(ULevel& level,
                              AS7LevelAuthoringDocument& document,
                              FS7LevelSyncPlan const& plan) -> std::expected<void, FString> {
    using namespace s7_level_reconciliation_detail;
    auto current{make_changes(level, document, plan.definition)};
    auto const& definition{plan.definition.components()};
    if (!current) {
        return std::unexpected{current.error()};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "Apply", "Apply S7 Level to Scene")};
    TMap<FName, AActor*> resolved;
    for (auto const& binding : document.entities) {
        resolved.Add(binding.id, binding.actor);
    }
    auto const& entities{definition.entities};
    TArray<AActor*> staged;
    TArray<AActor*> removed;

    for (auto const& change : current->changes) {
        auto const id{name(change.id)};
        if (change.action == ES7LevelSyncAction::Remove) {
            removed.Add(resolved.FindChecked(id));
            resolved.Remove(id);
            continue;
        }
        if (change.action != ES7LevelSyncAction::Add &&
            change.action != ES7LevelSyncAction::Replace) {
            continue;
        }
        auto const& entity{entities[change.incoming_index]};
        auto* const actor{
            GEditor->AddActor(&level,
                              s7_level_actor_class(entity.archetype, *document.level_config),
                              transform(entity),
                              true,
                              RF_Transactional,
                              false)};
        if (!IsValid(actor)) {
            for (auto* const spawned : staged) {
                spawned->Destroy();
            }
            transaction.Cancel();
            return std::unexpected{
                FString::Printf(TEXT("Could not spawn entity '%s'."), *id.ToString())};
        }
        staged.Add(actor);
        if (change.action == ES7LevelSyncAction::Replace) {
            removed.Add(resolved.FindChecked(id));
        }
        resolved.Add(id, actor);
    }

    for (auto const& change : current->changes) {
        if (change.action == ES7LevelSyncAction::Remove) {
            continue;
        }
        auto const id{name(change.id)};
        auto& actor{*resolved.FindChecked(id)};
        if (change.configure_actor) {
            auto const& entity{entities[change.incoming_index]};
            configure_s7_level_actor(actor,
                                     entity.archetype,
                                     ioj::to_unreal(entity.team),
                                     *document.level_config,
                                     transform(entity),
                                     id);
        } else if (change.relabel_actor) {
            actor.Modify();
            actor.SetActorLabel(id.ToString(), true);
        }
    }
    for (auto* const actor : removed) {
        actor->Modify();
        actor->Destroy();
    }

    document.Modify();
    auto const grid{definition.collision_grid.value_or(levels::LevelCollisionGridDefinition{})};
    auto vector{[](std::optional<ioj::Vector3d> const& value) -> FVector3f {
        return value ? FVector3f{static_cast<float>(value->x()),
                                 static_cast<float>(value->y()),
                                 static_cast<float>(value->z())}
                     : FVector3f::ZeroVector;
    }};
    document.level_size = vector(grid.level_size);
    document.grid_cell_size = vector(grid.cell_size);
    document.grid_override_schema_version = 1;
    document.level_id = FName{ioj::to_fstring(definition.metadata.id.value)};
    document.title = ioj::to_fstring(definition.metadata.title);
    document.description = ioj::to_fstring(definition.metadata.description);
    document.entities.Reset(static_cast<int32>(entities.size()));
    for (auto const& entity : entities) {
        auto const id{name(entity.id)};
        document.entities.Add({.id = id,
                               .actor = resolved.FindChecked(id),
                               .spawn_time_seconds = entity.spawn_time_seconds});
    }
    document.use_observer_camera = definition.camera.has_value();
    document.camera.targets.Reset();
    if (definition.camera.has_value()) {
        auto const& camera{*definition.camera};
        document.camera.offset_direction = FVector{
            camera.offset_direction.x(), camera.offset_direction.y(), camera.offset_direction.z()};
        document.camera.distance = camera.distance;
        for (auto const& id : camera.target_entity_ids) {
            document.camera.targets.Add(resolved.FindChecked(name(id)));
        }
    }
    document.mission = {};
    if (definition.mission.has_value()) {
        auto const& mission{*definition.mission};
        document.mission.mode = s7_mission_mode(mission.mode);
        if (mission.time_limit_seconds.has_value()) {
            document.mission.time_limit_seconds = *mission.time_limit_seconds;
        }
        document.mission.use_explicit_kill_count = mission.kill_count.has_value();
        if (mission.kill_count.has_value()) {
            document.mission.kill_count = *mission.kill_count;
        }
        auto append_actors = [&resolved](TArray<TObjectPtr<AActor>>& output,
                                         std::vector<levels::EntityId> const& ids) {
            for (auto const& id : ids) {
                output.Add(resolved.FindChecked(name(id)));
            }
        };
        append_actors(document.mission.heroes, mission.hero_entity_ids);
        append_actors(document.mission.must_survive, mission.must_survive_entity_ids);
        append_actors(document.mission.required_kills, mission.required_kill_entity_ids);
    }
    return {};
}
}

#include "SandboxEditor/levels/S7LevelReconciliation.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <SpaceGame/levels/LevelEntityTableOperations.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <LevelUtils.h>
#include <ScopedTransaction.h>

namespace ml::editor {
auto make_s7_level_sync_plan(ULevel const& level,
                             AS7LevelAuthoringDocument const& document,
                             FLevelDefinition const& definition)
    -> std::expected<FS7LevelSyncPlan, FString> {
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

    auto native{::ioj::levels::authoring::to_native(definition)};
    if (!native) {
        return std::unexpected{ml::to_fstring(::ioj::levels::format_diagnostics(native.error()))};
    }
    // Validate every configured class before any actor is spawned or modified.
    for (auto const& entity : native->entities) {
        auto* const type{s7_level_actor_class(entity.archetype, *document.level_config)};
        if (!IsValid(type) ||
            type->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient)) {
            return std::unexpected{FString::Printf(TEXT("Entity '%s' cannot be materialised."),
                                                   *ml::to_fstring(entity.id.value))};
        }
    }
    auto scene{collect_s7_scene(level, document)};
    if (!scene) {
        return std::unexpected{scene.error()};
    }
    auto changes{::ioj::levels::authoring::reconcile_scene(*scene, *native)};
    if (!changes) {
        return std::unexpected{ml::to_fstring(::ioj::levels::format_diagnostics(changes.error()))};
    }
    FS7LevelSyncPlan result;
    static_cast<::ioj::levels::authoring::SyncPlan&>(result) = MoveTemp(*changes);
    result.definition = definition;
    return result;
}

auto apply_s7_level_sync_plan(ULevel& level,
                              AS7LevelAuthoringDocument& document,
                              FS7LevelSyncPlan const& plan) -> std::expected<void, FString> {
    auto current{make_s7_level_sync_plan(level, document, plan.definition)};
    if (!current) {
        return std::unexpected{current.error()};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "Apply", "Apply S7 Level to Scene")};
    TMap<FName, AActor*> resolved;
    for (auto const& binding : document.entities) {
        resolved.Add(binding.id, binding.actor);
    }
    auto const entities{plan.definition.entities.get_const_view()};
    TArray<AActor*> staged;
    TArray<AActor*> removed;

    for (auto const& change : current->changes) {
        auto const id{FName{ml::to_fstring(change.id.value)}};
        if (change.action == ES7LevelSyncAction::Remove) {
            removed.Add(resolved.FindChecked(id));
            resolved.Remove(id);
            continue;
        }
        if (change.action != ES7LevelSyncAction::Add &&
            change.action != ES7LevelSyncAction::Replace) {
            continue;
        }
        auto const index{static_cast<int32>(change.incoming_index)};
        auto const entity{level_entity_table_detail::get(entities, index)};
        auto const archetype{resolve_level_archetype(entity.archetype).GetValue()};
        auto* const actor{GEditor->AddActor(&level,
                                            s7_level_actor_class(archetype, *document.level_config),
                                            FTransform{entity.rotation, entity.position},
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
        auto const id{FName{ml::to_fstring(change.id.value)}};
        auto& actor{*resolved.FindChecked(id)};
        if (change.configure_actor) {
            auto const index{static_cast<int32>(change.incoming_index)};
            auto const entity{level_entity_table_detail::get(entities, index)};
            configure_s7_level_actor(actor,
                                     resolve_level_archetype(entity.archetype).GetValue(),
                                     ml::to_unreal(entity.team),
                                     *document.level_config,
                                     FTransform{entity.rotation, entity.position},
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
    auto const grid{plan.definition.collision_grid.IsSet()
                        ? plan.definition.collision_grid.GetValue()
                        : FLevelCollisionGridDefinition{}};
    document.level_size =
        grid.level_size.IsSet() ? grid.level_size.GetValue() : FVector3f::ZeroVector;
    document.grid_cell_size =
        grid.cell_size.IsSet() ? grid.cell_size.GetValue() : FVector3f::ZeroVector;
    document.grid_override_schema_version = 1;
    auto const entity_count{entities.num()};
    document.level_id = plan.definition.metadata.id.value;
    document.title = plan.definition.metadata.title;
    document.description = plan.definition.metadata.description;
    document.entities.Reset(entity_count);
    for (int32 index{}; index < entity_count; ++index) {
        auto const id{entities.ids[index].value};
        document.entities.Add({.id = id,
                               .actor = resolved.FindChecked(id),
                               .spawn_time_seconds = entities.spawn_times_seconds[index]});
    }
    document.use_observer_camera = plan.definition.camera.IsSet();
    document.camera.targets.Reset();
    if (plan.definition.camera.IsSet()) {
        auto const& camera{plan.definition.camera.GetValue()};
        document.camera.offset_direction = camera.offset_direction;
        document.camera.distance = camera.distance;
        for (auto const id : camera.target_entity_ids) {
            document.camera.targets.Add(resolved.FindChecked(id.value));
        }
    }
    document.mission = {};
    if (plan.definition.mission.IsSet()) {
        auto const& mission{plan.definition.mission.GetValue()};
        document.mission.mode = s7_mission_mode(mission.mode);
        if (mission.time_limit_seconds.IsSet()) {
            document.mission.time_limit_seconds = mission.time_limit_seconds.GetValue();
        }
        document.mission.use_explicit_kill_count = mission.kill_count.IsSet();
        if (mission.kill_count.IsSet()) {
            document.mission.kill_count = mission.kill_count.GetValue();
        }
        auto append_actors = [&resolved](TArray<TObjectPtr<AActor>>& output,
                                         TArray<FLevelEntityId> const& ids) {
            for (auto const id : ids) {
                output.Add(resolved.FindChecked(id.value));
            }
        };
        append_actors(document.mission.heroes, mission.hero_entity_ids);
        append_actors(document.mission.must_survive, mission.must_survive_entity_ids);
        append_actors(document.mission.required_kills, mission.required_kill_entity_ids);
    }
    return {};
}
}

#include "SandboxEditor/levels/S7LevelReconciliation.h"

#include "SandboxEditor/levels/S7LevelAuthoringActors.h"
#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include <ioj/levels/authoring/editor_support.h>
#include <ioj/levels/authoring/scene_reconciliation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Editor.h>
#include <Engine/Level.h>
#include <LevelUtils.h>
#include <ScopedTransaction.h>

namespace ioj::editor {
namespace s7_level_reconciliation_detail {
struct PreparedSync {
    UEditorEngine& editor;
    USpaceGameLevelConfig& config;
    levels::authoring::SyncPlan plan;
    std::vector<UClass*> entity_classes;
};

auto prepare_sync(ULevel const& level,
                  AS7LevelAuthoringDocument const& document,
                  levels::LevelDefinition const& incoming) -> std::expected<PreparedSync, FString> {
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

    auto& config{*document.level_config};
    std::vector<UClass*> entity_classes;
    entity_classes.reserve(incoming.entities().size());
    for (auto const& entity : incoming.entities()) {
        auto* const type{s7_level_actor_class(entity.archetype, config)};
        if (!IsValid(type) ||
            type->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient)) {
            return std::unexpected{FString::Printf(TEXT("Entity '%s' cannot be materialised."),
                                                   *to_fstring(entity.id.value))};
        }
        entity_classes.push_back(type);
    }

    auto scene{collect_s7_scene(level, document)};
    if (!scene) {
        return std::unexpected{scene.error()};
    }
    return PreparedSync{*GEditor,
                        config,
                        levels::authoring::reconcile_scene(*scene, incoming),
                        std::move(entity_classes)};
}

auto transform(levels::EntitySpawnDefinition const& entity) -> FTransform {
    return FTransform{FRotator{entity.rotation.pitch, entity.rotation.yaw, entity.rotation.roll},
                      ioj::to_unreal(entity.position)};
}
auto name(levels::EntityId const& id) -> FName {
    return FName{to_fstring(id.value)};
}
}

auto make_s7_level_sync_plan(ULevel const& level,
                             AS7LevelAuthoringDocument const& document,
                             levels::LevelDefinition definition)
    -> std::expected<FS7LevelSyncPlan, FString> {
    auto support{levels::authoring::check_editor_support(definition)};
    if (!support) {
        return std::unexpected{to_fstring(levels::format_diagnostics(support.error()))};
    }
    auto changes{s7_level_reconciliation_detail::prepare_sync(level, document, definition)};
    if (!changes) {
        return std::unexpected{changes.error()};
    }
    return FS7LevelSyncPlan{MoveTemp(changes->plan), MoveTemp(definition)};
}

auto apply_s7_level_sync_plan(ULevel& level,
                              AS7LevelAuthoringDocument& document,
                              FS7LevelSyncPlan const& plan) -> std::expected<void, FString> {
    using namespace s7_level_reconciliation_detail;
    // Recheck mutable editor state after the preview, before opening the transaction.
    auto current{prepare_sync(level, document, plan.definition)};
    auto const& definition{plan.definition};
    if (!current) {
        return std::unexpected{current.error()};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7LevelAuthoring", "Apply", "Apply S7 Level to Scene")};
    TMap<FName, AActor*> resolved;
    for (auto const& binding : document.entities) {
        resolved.Add(binding.id, binding.actor);
    }
    auto const& entities{definition.entities()};
    TArray<AActor*> staged;
    TArray<AActor*> removed;

    for (auto const& change : current->plan.changes) {
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
        auto* const actor{current->editor.AddActor(&level,
                                                   current->entity_classes[change.incoming_index],
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

    for (auto const& change : current->plan.changes) {
        if (change.action == ES7LevelSyncAction::Remove) {
            continue;
        }
        auto const id{name(change.id)};
        auto& actor{*resolved.FindChecked(id)};
        if (change.configure_actor) {
            auto const& entity{entities[change.incoming_index]};
            configure_s7_level_actor(actor,
                                     entity.archetype,
                                     to_unreal(entity.team),
                                     current->config,
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
    auto const grid{definition.collision_grid().value_or(levels::LevelCollisionGridDefinition{})};
    document.level_size = to_unreal(Vector3f{grid.level_size.value_or(Vector3d{})});
    document.grid_cell_size = to_unreal(Vector3f{grid.cell_size.value_or(Vector3d{})});
    document.grid_override_schema_version = 1;
    document.level_id = FName{to_fstring(definition.metadata().id.value)};
    document.title = to_fstring(definition.metadata().title);
    document.description = to_fstring(definition.metadata().description);
    document.entities.Reset(static_cast<int32>(entities.size()));
    for (auto const& entity : entities) {
        auto const id{name(entity.id)};
        document.entities.Add({.id = id,
                               .actor = resolved.FindChecked(id),
                               .spawn_time_seconds = entity.spawn_time_seconds});
    }
    document.use_observer_camera = definition.camera().has_value();
    document.camera.targets.Reset();
    if (definition.camera().has_value()) {
        auto const& camera{*definition.camera()};
        document.camera.offset_direction = FVector{
            camera.offset_direction.x(), camera.offset_direction.y(), camera.offset_direction.z()};
        document.camera.distance = camera.distance;
        for (auto const& id : camera.target_entity_ids) {
            document.camera.targets.Add(resolved.FindChecked(name(id)));
        }
    }

    return {};
}
}

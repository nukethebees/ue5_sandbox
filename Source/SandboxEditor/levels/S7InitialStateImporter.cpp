#include "SandboxEditor/levels/S7InitialStateImporter.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelScriptCatalog.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <DesktopPlatformModule.h>
#include <Editor.h>
#include <Editor/EditorEngine.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Actor.h>
#include <IDesktopPlatform.h>
#include <LevelUtils.h>
#include <ScopedTransaction.h>
#include <UObject/SoftObjectPath.h>

namespace ml::editor {
namespace {
struct FDelayedSpawnGroup {
    double time_seconds{};
    EResolvedLevelArchetype archetype{};
};

auto count_text(int32 const count, FStringView const singular, FStringView const plural)
    -> FString {
    return FString::Printf(TEXT("%d %.*s"),
                           count,
                           count == 1 ? singular.Len() : plural.Len(),
                           count == 1 ? singular.GetData() : plural.GetData());
}

auto format_read_error(s7::FLevelDefinitionReadResult const& result) -> FString {
    if (!result.script_error.IsEmpty()) {
        return result.script_error;
    }

    TArray<FString> messages;
    messages.Reserve(result.decode_errors.Num() + result.validation_errors.Num());
    for (auto const& error : result.decode_errors) {
        messages.Add(FString::Printf(TEXT("%s: %s"), *error.path, *error.message));
    }
    for (auto const& error : result.validation_errors) {
        messages.Add(error.message);
    }
    return FString::Join(messages, TEXT("\n"));
}

auto select_level_script() -> TOptional<FString> {
    auto* const desktop_platform{FDesktopPlatformModule::Get()};
    if (!desktop_platform) {
        return NullOpt;
    }

    auto const parent_window{
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)};
    TArray<FString> filenames;
    auto const selected{desktop_platform->OpenFileDialog(parent_window,
                                                         TEXT("Import S7 Initial State"),
                                                         s7::default_level_script_directory(),
                                                         TEXT(""),
                                                         TEXT("S7 level (*.scm)|*.scm"),
                                                         EFileDialogFlags::None,
                                                         filenames)};
    return selected && filenames.Num() == 1 ? TOptional<FString>{MoveTemp(filenames[0])} : NullOpt;
}

auto resolve_level_config(UWorld& world) -> std::expected<USpaceGameLevelConfig*, FString> {
    ATestBatchOrchestrator* orchestrator{};
    for (TActorIterator<ATestBatchOrchestrator> it{&world}; it; ++it) {
        if (orchestrator) {
            return std::unexpected{
                FString{TEXT("The current map contains more than one batch orchestrator.")}};
        }
        orchestrator = *it;
    }

    if (IsValid(orchestrator)) {
        auto* const config{const_cast<USpaceGameLevelConfig*>(orchestrator->get_level_config())};
        if (!IsValid(config)) {
            return std::unexpected{
                FString{TEXT("The current map's orchestrator has no level configuration.")}};
        }
        return config;
    }

    constexpr TCHAR runtime_config_path[]{
        TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig")};
    auto* const config{LoadObject<USpaceGameLevelConfig>(nullptr, runtime_config_path)};
    if (!IsValid(config)) {
        return std::unexpected{FString::Printf(
            TEXT("Could not load the fallback level configuration '%s'."), runtime_config_path)};
    }
    return config;
}

auto actor_class_for(EResolvedLevelArchetype const archetype, USpaceGameLevelConfig const& config)
    -> UClass* {
    switch (archetype) {
        case EResolvedLevelArchetype::PlayerFighter:
            return config.classes.player_ship_class.Get();
        case EResolvedLevelArchetype::CapitalShip:
            return config.classes.capital_ship_proxy_class.Get();
        case EResolvedLevelArchetype::StaticTurret:
            return config.classes.static_turret_proxy_class.Get();
    }
    return nullptr;
}

auto archetype_name(EResolvedLevelArchetype const archetype) -> FStringView {
    switch (archetype) {
        case EResolvedLevelArchetype::PlayerFighter:
            return TEXTVIEW("player-fighter");
        case EResolvedLevelArchetype::CapitalShip:
            return TEXTVIEW("capital-ship");
        case EResolvedLevelArchetype::StaticTurret:
            return TEXTVIEW("static-turret");
    }
    return TEXTVIEW("unknown");
}

auto validate_actor_classes(FS7InitialStateImportPlan const& plan,
                            USpaceGameLevelConfig const& config) -> FString {
    for (auto const& entity : plan.entities) {
        auto const* const actor_class{actor_class_for(entity.archetype, config)};
        if (!IsValid(actor_class)) {
            auto const name{archetype_name(entity.archetype)};
            return FString::Printf(TEXT("The level configuration has no actor class for '%.*s'."),
                                   name.Len(),
                                   name.GetData());
        }
        if (actor_class->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable | CLASS_Transient)) {
            return FString::Printf(TEXT("Actor class '%s' cannot be placed in an editor level."),
                                   *actor_class->GetPathName());
        }
    }
    return {};
}

void configure_actor(AActor& actor,
                     FS7InitialStateEntity const& entity,
                     USpaceGameLevelConfig& config) {
    actor.Modify();
    switch (entity.archetype) {
        case EResolvedLevelArchetype::PlayerFighter: {
            auto* const player{CastChecked<ATestSpaceShip>(&actor)};
            player->set_actor_config(&config.player_ship);
            player->set_team(entity.team);
            break;
        }
        case EResolvedLevelArchetype::CapitalShip: {
            auto* const capital{CastChecked<ATestCapitalShipProxy>(&actor)};
            capital->set_level_config_asset(&config);
            capital->set_team(entity.team);
            break;
        }
        case EResolvedLevelArchetype::StaticTurret: {
            auto* const turret{CastChecked<ATestStaticTurretsProxy>(&actor)};
            turret->set_actor_config(&config.turrets);
            turret->set_team(entity.team);
            break;
        }
    }

    actor.RerunConstructionScripts();
    actor.PostEditMove(true);
    actor.SetActorLabel(entity.id.value.ToString(), true);
}

void show_error(FString const& message) {
    UE_LOG(LogSandbox, Error, TEXT("S7 initial-state import failed: %s"), *message);
}
}

auto FS7UnsupportedFeatureSummary::is_empty() const noexcept -> bool {
    return scheduled_spawn_group_count == 0 && scheduled_entity_count == 0 &&
           mission_definition_count == 0 && mission_event_count == 0 && initial_camera_count == 0 &&
           unlock_criterion_count == 0;
}

auto FS7UnsupportedFeatureSummary::format() const -> FString {
    TArray<FString> lines;
    if (scheduled_entity_count > 0) {
        lines.Add(FString::Printf(
            TEXT("- %s (%s)"),
            *count_text(scheduled_spawn_group_count,
                        TEXTVIEW("scheduled spawn group"),
                        TEXTVIEW("scheduled spawn groups")),
            *count_text(scheduled_entity_count, TEXTVIEW("entity"), TEXTVIEW("entities"))));
    }
    if (mission_definition_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s"),
                                  *count_text(mission_definition_count,
                                              TEXTVIEW("mission definition"),
                                              TEXTVIEW("mission definitions"))));
    }
    if (mission_event_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s"),
                                  *count_text(mission_event_count,
                                              TEXTVIEW("scheduled mission event"),
                                              TEXTVIEW("scheduled mission events"))));
    }
    if (initial_camera_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s"),
                                  *count_text(initial_camera_count,
                                              TEXTVIEW("initial camera definition"),
                                              TEXTVIEW("initial camera definitions"))));
    }
    if (unlock_criterion_count > 0) {
        lines.Add(FString::Printf(TEXT("- %s"),
                                  *count_text(unlock_criterion_count,
                                              TEXTVIEW("unlock criterion"),
                                              TEXTVIEW("unlock criteria"))));
    }
    return FString::Join(lines, TEXT("\n"));
}

auto make_s7_initial_state_import_plan(FLevelDefinition const& definition)
    -> std::expected<FS7InitialStateImportPlan, FString> {
    auto const validation{validate_level(definition)};
    if (!validation) {
        TArray<FString> errors;
        errors.Reserve(validation.errors.Num());
        for (auto const& error : validation.errors) {
            errors.Add(error.message);
        }
        return std::unexpected{FString::Join(errors, TEXT("\n"))};
    }

    FS7InitialStateImportPlan plan{
        .level_id = definition.metadata.id,
        .level_title = definition.metadata.title,
    };
    plan.unsupported.mission_definition_count = definition.mission.IsSet() ? 1 : 0;
    plan.unsupported.mission_event_count = definition.mission_events.Num();
    plan.unsupported.initial_camera_count = definition.camera.IsSet() ? 1 : 0;
    plan.unsupported.unlock_criterion_count = definition.unlock_criteria.Num();

    TArray<FDelayedSpawnGroup> delayed_groups;
    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    plan.entities.Reserve(entity_count);
    for (int32 i{}; i < entity_count; ++i) {
        auto const archetype{resolve_level_archetype(entities.archetypes[i])};
        auto const team{resolve_level_team(entities.teams[i])};
        check(archetype.IsSet() && team.IsSet());

        auto const spawn_time{entities.spawn_times_seconds[i]};
        if (spawn_time > 0.0) {
            ++plan.unsupported.scheduled_entity_count;
            if (!delayed_groups.ContainsByPredicate([&](FDelayedSpawnGroup const& group) {
                    return group.time_seconds == spawn_time &&
                           group.archetype == archetype.GetValue();
                })) {
                delayed_groups.Add({.time_seconds = spawn_time, .archetype = archetype.GetValue()});
            }
            continue;
        }

        plan.entities.Add({
            .id = entities.ids[i],
            .archetype = archetype.GetValue(),
            .team = team.GetValue(),
            .transform = FTransform{FRotator{entities.rotations.pitches[i],
                                             entities.rotations.yaws[i],
                                             entities.rotations.rolls[i]},
                                    FVector{entities.positions.xs[i],
                                            entities.positions.ys[i],
                                            entities.positions.zs[i]}},
        });
    }
    plan.unsupported.scheduled_spawn_group_count = delayed_groups.Num();
    return plan;
}

auto materialise_s7_initial_state(ULevel& level,
                                  USpaceGameLevelConfig& config,
                                  FS7InitialStateImportPlan const& plan)
    -> FS7InitialStateMaterialisationResult {
    if (!GEditor) {
        return {.error = TEXT("GEditor is unavailable.")};
    }
    if (!IsValid(level.OwningWorld) || level.OwningWorld->IsGameWorld()) {
        return {.error = TEXT("The target is not an editor world.")};
    }
    if (FLevelUtils::IsLevelLocked(&level)) {
        return {.error = TEXT("The current editor level is locked.")};
    }
    if (auto const error{validate_actor_classes(plan, config)}; !error.IsEmpty()) {
        return {.error = error};
    }

    FScopedTransaction transaction{
        NSLOCTEXT("S7InitialStateImporter", "Materialise", "Import S7 Initial State")};
    TArray<AActor*> actors;
    actors.Reserve(plan.entities.Num());
    for (auto const& entity : plan.entities) {
        auto* const actor{GEditor->AddActor(&level,
                                            actor_class_for(entity.archetype, config),
                                            entity.transform,
                                            true,
                                            RF_Transactional,
                                            false)};
        if (!IsValid(actor)) {
            for (auto* const added_actor : actors) {
                if (IsValid(added_actor)) {
                    added_actor->Destroy();
                }
            }
            transaction.Cancel();
            return {.error = FString::Printf(TEXT("Failed to spawn entity '%s'."),
                                             *entity.id.value.ToString())};
        }

        configure_actor(*actor, entity, config);
        actors.Add(actor);
    }

    return {.imported_entity_count = actors.Num()};
}

void execute_s7_initial_state_import() {
    auto const selected_path{select_level_script()};
    if (!selected_path.IsSet()) {
        return;
    }
    if (!GEditor || IsValid(GEditor->PlayWorld)) {
        show_error(TEXT("Import is available only while editing a level outside Play In Editor."));
        return;
    }

    auto* const world{GEditor->GetEditorWorldContext().World()};
    auto* const level{IsValid(world) ? world->GetCurrentLevel() : nullptr};
    if (!IsValid(world) || !IsValid(level)) {
        show_error(TEXT("The current editor level is unavailable."));
        return;
    }

    s7::FLevelDefinitionReader reader;
    auto const read_result{reader.read_file(selected_path.GetValue())};
    if (!read_result) {
        show_error(format_read_error(read_result));
        return;
    }

    auto const plan{make_s7_initial_state_import_plan(read_result.definition.GetValue())};
    if (!plan) {
        show_error(plan.error());
        return;
    }

    auto const config{resolve_level_config(*world)};
    if (!config) {
        show_error(config.error());
        return;
    }

    auto const result{materialise_s7_initial_state(*level, **config, *plan)};
    if (!result) {
        show_error(result.error);
        return;
    }

    FString summary{FString::Printf(TEXT("Imported %d entities."), result.imported_entity_count)};
    if (!plan->unsupported.is_empty()) {
        summary += TEXT("\n\nUnsupported level features:\n");
        summary += plan->unsupported.format();
    }
    if (plan->unsupported.is_empty()) {
        UE_LOG(LogSandbox, Display, TEXT("%s"), *summary);
    } else {
        UE_LOG(LogSandbox, Warning, TEXT("%s"), *summary);
    }
}
}

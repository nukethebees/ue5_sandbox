#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringMode.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <CQTest.h>
#include <Editor.h>
#include <EditorModeManager.h>
#include <Engine/World.h>
#include <Tests/AutomationEditorCommon.h>

namespace {
template <typename T>
auto spawn(UWorld& world, TCHAR const* label) -> T* {
    auto* const actor{world.SpawnActor<T>()};
    if (IsValid(actor)) {
        actor->SetActorLabel(label, true);
    }
    return actor;
}

auto config(UObject& outer) -> USpaceGameLevelConfig* {
    auto* const source{LoadObject<USpaceGameLevelConfig>(
        nullptr, TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"))};
    return IsValid(source) ? DuplicateObject<USpaceGameLevelConfig>(source, &outer) : nullptr;
}
}

TEST_CLASS(S7LevelAuthoring, "Sandbox.UnitTests")
{
    TEST_METHOD(ObjectivesRoundTripThroughDocumentAndWriter)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const player{spawn<ATestSpaceShip>(*world, TEXT("Player"))};
        auto* const enemy{spawn<ATestCapitalShipProxy>(*world, TEXT("Enemy"))};
        if (!TestRunner->TestNotNull(TEXT("World"), world) ||
            !TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Player"), player) ||
            !TestRunner->TestNotNull(TEXT("Enemy"), enemy)) {
            return;
        }
        player->set_team(ETestTeam::Blue);
        enemy->set_team(ETestTeam::Red);
        document->level_id = TEXT("objective-round-trip");
        document->title = TEXT("Objective Round Trip");
        document->entities = {{.id = TEXT("player"), .actor = player},
                              {.id = TEXT("enemy"), .actor = enemy}};
        document->mission.mode = ETestMissionMode::KillEnemies;
        document->mission.heroes = {player};
        document->mission.must_survive = {player};
        document->mission.required_kills = {enemy};

        auto const definition{
            ml::editor::collect_s7_editor_level(*world->GetCurrentLevel(), *document)};
        if (!TestRunner->TestTrue(TEXT("Document collects"), definition.has_value())) {
            TestRunner->AddError(definition.error());
            return;
        }
        auto const source{ml::s7::emit_editor_level_source(*definition)};
        if (!TestRunner->TestTrue(TEXT("Definition writes"), source.has_value())) {
            return;
        }
        ml::s7::FLevelDefinitionReader reader;
        auto const read{reader.read_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Generated source reads"), static_cast<bool>(read))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Mission survives"), read.definition->mission.IsSet());
        TestRunner->TestEqual(TEXT("Required kill survives"),
                              read.definition->mission->required_kill_entity_ids.Num(),
                              1);
    }

    TEST_METHOD(PreviewAndApplyReconcileManagedActors)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        auto* const old_actor{spawn<ATestCapitalShipProxy>(*world, TEXT("Old"))};
        auto* const level_config{config(*world)};
        if (!TestRunner->TestNotNull(TEXT("Document"), document) ||
            !TestRunner->TestNotNull(TEXT("Old actor"), old_actor) ||
            !TestRunner->TestNotNull(TEXT("Config"), level_config)) {
            return;
        }
        document->level_config = level_config;
        document->entities = {{.id = TEXT("old"), .actor = old_actor}};

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("sync")}, .title = TEXT("Sync")});
        builder.add_team(ml::level_teams::blue);
        builder.set_player_entity(ml::FLevelEntityId{TEXT("player")});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("player")},
                            .archetype = ml::level_archetypes::player_fighter,
                            .team = ml::level_teams::blue,
                            .position = FVector{100.0, 200.0, 300.0}});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        if (!TestRunner->TestTrue(TEXT("Preview builds"), plan.has_value())) {
            TestRunner->AddError(plan.error());
            return;
        }
        TestRunner->TestEqual(
            TEXT("One actor adds"), plan->count(ml::editor::ES7LevelSyncAction::Add), 1);
        TestRunner->TestEqual(
            TEXT("One actor removes"), plan->count(ml::editor::ES7LevelSyncAction::Remove), 1);
        auto const applied{
            ml::editor::apply_s7_level_sync_plan(*world->GetCurrentLevel(), *document, *plan)};
        TestRunner->TestTrue(TEXT("Preview applies"), applied.has_value());
        TestRunner->TestEqual(TEXT("One binding remains"), document->entities.Num(), 1);
        TestRunner->TestEqual(
            TEXT("Binding uses script id"), document->entities[0].id, FName{TEXT("player")});
        TestRunner->TestTrue(TEXT("Removed actor is invalid"), !IsValid(old_actor));
    }

    TEST_METHOD(RejectsOutOfScopeRuntimeFeatures)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const document{spawn<AS7LevelAuthoringDocument>(*world, TEXT("S7 Document"))};
        document->level_config = config(*world);

        ml::FLevelBuilder builder;
        builder.set_metadata({.id = ml::FLevelId{TEXT("delayed")}, .title = TEXT("Delayed")});
        builder.add_team(ml::level_teams::blue);
        builder.set_camera({.target_entity_ids = {ml::FLevelEntityId{TEXT("ship")}},
                            .offset_direction = FVector{-1.0, 0.0, 0.0},
                            .distance = 1000.0});
        builder.add_entity({.id = ml::FLevelEntityId{TEXT("ship")},
                            .archetype = ml::level_archetypes::capital_ship,
                            .team = ml::level_teams::blue,
                            .spawn_time_seconds = 1.0});
        auto const plan{ml::editor::make_s7_level_sync_plan(
            *world->GetCurrentLevel(), *document, builder.finish())};
        TestRunner->TestFalse(TEXT("Delayed spawn is rejected"), plan.has_value());
    }

    TEST_METHOD(EditorModeActivatesAndDeactivates)
    {
        auto& modes{GLevelEditorModeTools()};
        modes.ActivateMode(US7LevelAuthoringMode::mode_id);
        TestRunner->TestTrue(TEXT("Mode activates"),
                             modes.IsModeActive(US7LevelAuthoringMode::mode_id));
        modes.DeactivateMode(US7LevelAuthoringMode::mode_id);
        TestRunner->TestFalse(TEXT("Mode deactivates"),
                              modes.IsModeActive(US7LevelAuthoringMode::mode_id));
    }
};

#include <SandboxEditor/levels/S7InitialStateImporter.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>

namespace {
auto make_actor_level() -> ml::FLevelDefinition {
    ml::FLevelBuilder builder;
    builder.set_metadata({
        .id = ml::FLevelId{TEXT("editor-import")},
        .title = TEXT("Editor Import"),
    });
    builder.add_team(ml::level_teams::blue);
    builder.add_team(ml::level_teams::red);
    builder.set_player_entity(ml::FLevelEntityId{TEXT("player")});
    builder.add_entity({
        .id = ml::FLevelEntityId{TEXT("player")},
        .archetype = ml::level_archetypes::player_fighter,
        .team = ml::level_teams::blue,
        .position = FVector{100.0, 200.0, 300.0},
        .rotation = FRotator{10.0, 20.0, 30.0},
    });
    builder.add_entity({
        .id = ml::FLevelEntityId{TEXT("capital")},
        .archetype = ml::level_archetypes::capital_ship,
        .team = ml::level_teams::blue,
        .position = FVector{1000.0, 2000.0, 3000.0},
        .rotation = FRotator{0.0, 90.0, 0.0},
    });
    builder.add_entity({
        .id = ml::FLevelEntityId{TEXT("turret")},
        .archetype = ml::level_archetypes::static_turret,
        .team = ml::level_teams::red,
        .position = FVector{-1000.0, -2000.0, -3000.0},
        .rotation = FRotator{0.0, -90.0, 0.0},
    });
    return builder.finish();
}

template <typename T>
auto find_actor(UWorld& world, FString const& label) -> T* {
    for (TActorIterator<T> it{&world}; it; ++it) {
        if (it->GetActorLabel() == label) {
            return *it;
        }
    }
    return nullptr;
}

template <typename T>
auto count_actors(UWorld& world) -> int32 {
    int32 count{};
    for (TActorIterator<T> it{&world}; it; ++it) {
        ++count;
    }
    return count;
}
}

TEST_CLASS(S7InitialStateImporter, "Sandbox.UnitTests")
{
    TEST_METHOD(ProceduralS7ProducesInitialEntities)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const read_result{reader.read_source(LR"(
(define (make-ship index)
  (entity (string->symbol (format #f "ship-~A" index)) 'capital-ship 'blue
    (position (* index 1000) 200 300)
    (rotation 0 (* index 10) 0)))
(level
  (id 'procedural-import)
  (title "Procedural Import")
  (teams (team 'blue))
  (camera (look-at 'ship-0) (distance 1000) (offset-direction -1 0 0))
  (apply entities (map make-ship '(0 1 2 3))))
)")};
        if (!TestRunner->TestTrue(TEXT("Procedural S7 evaluates"),
                                  static_cast<bool>(read_result))) {
            return;
        }

        auto const plan{
            ml::editor::make_s7_initial_state_import_plan(read_result.definition.GetValue())};
        if (!TestRunner->TestTrue(TEXT("Import plan is created"), plan.has_value())) {
            return;
        }

        TestRunner->TestEqual(TEXT("All generated entities are retained"), plan->entities.Num(), 4);
        TestRunner->TestEqual(TEXT("Generated position is retained"),
                              plan->entities[3].transform.GetLocation(),
                              FVector{3000.0, 200.0, 300.0});
        TestRunner->TestEqual(TEXT("Generated rotation is retained"),
                              plan->entities[3].transform.Rotator(),
                              FRotator{0.0, 30.0, 0.0});
        TestRunner->TestEqual(
            TEXT("Camera is reported"), plan->unsupported.initial_camera_count, 1);
    }

    TEST_METHOD(UnsupportedFeaturesAreGrouped)
    {
        ml::s7::FLevelDefinitionReader reader;
        auto const read_result{reader.read_source(LR"(
(level
  (id 'unsupported-import)
  (title "Unsupported Import")
  (unlock (level-completed 'prior-level))
  (teams (team 'blue) (team 'red))
  (camera (look-at 'hero) (distance 1000) (offset-direction -1 0 0))
  (mission (mode 'kill-enemies) (kill-count 3) (heroes 'hero))
  (mission-events
    (mission-event (at 2) (add-required-kills 'enemy-a)))
  (entities
    (entity 'hero 'capital-ship 'blue
      (position 0 0 0) (rotation 0 0 0))
    (entity 'enemy-a 'capital-ship 'red
      (position 1000 0 0) (rotation 0 0 0) (spawn-at 1))
    (entity 'enemy-b 'capital-ship 'red
      (position 2000 0 0) (rotation 0 0 0) (spawn-at 1))
    (entity 'enemy-turret 'static-turret 'red
      (position 3000 0 0) (rotation 0 0 0) (spawn-at 1))))
)")};
        if (!TestRunner->TestTrue(TEXT("Unsupported fixture evaluates"),
                                  static_cast<bool>(read_result))) {
            return;
        }

        auto const plan{
            ml::editor::make_s7_initial_state_import_plan(read_result.definition.GetValue())};
        if (!TestRunner->TestTrue(TEXT("Import plan is created"), plan.has_value())) {
            return;
        }

        TestRunner->TestEqual(TEXT("Only the initial entity is retained"), plan->entities.Num(), 1);
        TestRunner->TestEqual(
            TEXT("Delayed entities are counted"), plan->unsupported.scheduled_entity_count, 3);
        TestRunner->TestEqual(TEXT("Delayed entities group by time and archetype"),
                              plan->unsupported.scheduled_spawn_group_count,
                              2);
        TestRunner->TestEqual(
            TEXT("Mission is reported"), plan->unsupported.mission_definition_count, 1);
        TestRunner->TestEqual(
            TEXT("Mission event is reported"), plan->unsupported.mission_event_count, 1);
        TestRunner->TestEqual(
            TEXT("Camera is reported"), plan->unsupported.initial_camera_count, 1);
        TestRunner->TestEqual(
            TEXT("Unlock is reported"), plan->unsupported.unlock_criterion_count, 1);
    }

    TEST_METHOD(CheckedInLevelsProduceRepresentativeInitialStates)
    {
        auto make_plan = [this](TCHAR const* filename) -> ml::editor::FS7InitialStateImportPlan {
            ml::s7::FLevelDefinitionReader reader;
            auto const path{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), filename)};
            auto const read_result{reader.read_file(path)};
            if (!TestRunner->TestTrue(*FString::Printf(TEXT("%s evaluates"), filename),
                                      static_cast<bool>(read_result))) {
                return {};
            }

            auto const plan{
                ml::editor::make_s7_initial_state_import_plan(read_result.definition.GetValue())};
            if (!TestRunner->TestTrue(
                    *FString::Printf(TEXT("%s produces an import plan"), filename),
                    plan.has_value())) {
                return {};
            }
            return *plan;
        };

        auto const static_level{make_plan(TEXT("BorderSkirmish.scm"))};
        TestRunner->TestEqual(
            TEXT("Static level imports all entities"), static_level.entities.Num(), 4);
        TestRunner->TestEqual(TEXT("Static level reports its mission"),
                              static_level.unsupported.mission_definition_count,
                              1);

        auto const large_level{make_plan(TEXT("BenchmarkFleet_10.scm"))};
        TestRunner->TestEqual(
            TEXT("Large procedural fleet imports every capital"), large_level.entities.Num(), 320);
        TestRunner->TestEqual(TEXT("Large fleet reports its camera"),
                              large_level.unsupported.initial_camera_count,
                              1);

        auto const procedural_level{make_plan(TEXT("SixFactionArmada.scm"))};
        TestRunner->TestEqual(TEXT("Procedural armada imports every generated entity"),
                              procedural_level.entities.Num(),
                              229);
        TestRunner->TestTrue(TEXT("Procedural armada has no unsupported features"),
                             procedural_level.unsupported.is_empty());

        auto const runtime_level{make_plan(TEXT("CounteroffensiveSecondEchelon.scm"))};
        TestRunner->TestEqual(
            TEXT("Runtime level retains initial entities"), runtime_level.entities.Num(), 7);
        TestRunner->TestEqual(TEXT("Runtime level reports its delayed spawn"),
                              runtime_level.unsupported.scheduled_entity_count,
                              1);
        TestRunner->TestEqual(TEXT("Runtime level groups its delayed spawn"),
                              runtime_level.unsupported.scheduled_spawn_group_count,
                              1);
        TestRunner->TestEqual(TEXT("Runtime level reports its mission"),
                              runtime_level.unsupported.mission_definition_count,
                              1);
        TestRunner->TestEqual(TEXT("Runtime level reports its mission event"),
                              runtime_level.unsupported.mission_event_count,
                              1);
        TestRunner->TestEqual(TEXT("Runtime level reports its unlock"),
                              runtime_level.unsupported.unlock_criterion_count,
                              1);
    }

    TEST_METHOD(MaterialisesConfiguredActorsAsOneTransaction)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const source_config{LoadObject<USpaceGameLevelConfig>(
            nullptr,
            TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Editor world is created"), world) ||
            !TestRunner->TestNotNull(TEXT("Runtime config loads"), source_config)) {
            return;
        }
        auto* const config{DuplicateObject<USpaceGameLevelConfig>(source_config, world)};
        auto const plan{ml::editor::make_s7_initial_state_import_plan(make_actor_level())};
        if (!TestRunner->TestTrue(TEXT("Actor import plan is created"), plan.has_value())) {
            return;
        }

        auto const result{
            ml::editor::materialise_s7_initial_state(*world->GetCurrentLevel(), *config, *plan)};
        if (!TestRunner->TestTrue(TEXT("Actors materialise"), static_cast<bool>(result))) {
            return;
        }
        TestRunner->TestEqual(TEXT("Three entities import"), result.imported_entity_count, 3);

        auto* const player{find_actor<ATestSpaceShip>(*world, TEXT("player"))};
        auto* const capital{find_actor<ATestCapitalShipProxy>(*world, TEXT("capital"))};
        auto* const turret{find_actor<ATestStaticTurretsProxy>(*world, TEXT("turret"))};
        if (!TestRunner->TestNotNull(TEXT("Player actor exists"), player) ||
            !TestRunner->TestNotNull(TEXT("Capital actor exists"), capital) ||
            !TestRunner->TestNotNull(TEXT("Turret actor exists"), turret)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Configured player class is used"),
                             player->IsA(config->classes.player_ship_class));
        TestRunner->TestTrue(TEXT("Configured capital class is used"),
                             capital->IsA(config->classes.capital_ship_proxy_class));
        TestRunner->TestTrue(TEXT("Configured turret class is used"),
                             turret->IsA(config->classes.static_turret_proxy_class));
        TestRunner->TestEqual(TEXT("Player team is retained"), player->get_team(), ETestTeam::Blue);
        TestRunner->TestEqual(
            TEXT("Capital team is retained"), capital->get_team(), ETestTeam::Blue);
        TestRunner->TestEqual(TEXT("Turret team is retained"), turret->get_team(), ETestTeam::Red);
        TestRunner->TestTrue(TEXT("Capital transform is retained"),
                             capital->GetActorTransform().Equals(FTransform{
                                 FRotator{0.0, 90.0, 0.0}, FVector{1000.0, 2000.0, 3000.0}}));

        GEditor->UndoTransaction();
        TestRunner->TestEqual(
            TEXT("One undo removes the player"), count_actors<ATestSpaceShip>(*world), 0);
        TestRunner->TestEqual(
            TEXT("One undo removes the capital"), count_actors<ATestCapitalShipProxy>(*world), 0);
        TestRunner->TestEqual(
            TEXT("One undo removes the turret"), count_actors<ATestStaticTurretsProxy>(*world), 0);
    }

    TEST_METHOD(InvalidActorConfigurationAddsNothing)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const source_config{LoadObject<USpaceGameLevelConfig>(
            nullptr,
            TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Editor world is created"), world) ||
            !TestRunner->TestNotNull(TEXT("Runtime config loads"), source_config)) {
            return;
        }
        auto* const config{DuplicateObject<USpaceGameLevelConfig>(source_config, world)};
        config->classes.capital_ship_proxy_class = nullptr;
        auto const plan{ml::editor::make_s7_initial_state_import_plan(make_actor_level())};
        if (!TestRunner->TestTrue(TEXT("Actor import plan is created"), plan.has_value())) {
            return;
        }

        auto const result{
            ml::editor::materialise_s7_initial_state(*world->GetCurrentLevel(), *config, *plan)};
        TestRunner->TestFalse(TEXT("Invalid actor config is rejected"), static_cast<bool>(result));
        TestRunner->TestEqual(TEXT("No player is added"), count_actors<ATestSpaceShip>(*world), 0);
        TestRunner->TestEqual(
            TEXT("No capital is added"), count_actors<ATestCapitalShipProxy>(*world), 0);
        TestRunner->TestEqual(
            TEXT("No turret is added"), count_actors<ATestStaticTurretsProxy>(*world), 0);
    }
};

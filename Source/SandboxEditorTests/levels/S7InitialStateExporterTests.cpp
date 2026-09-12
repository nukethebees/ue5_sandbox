#include <SandboxEditor/levels/S7InitialStateExporter.h>
#include <SandboxEditor/levels/S7InitialStateImporter.h>

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionWriter.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <Tests/AutomationEditorCommon.h>

namespace {
auto export_metadata() -> ml::FLevelMetadata {
    return {
        .id = ml::FLevelId{TEXT("editor-export")},
        .title = TEXT("Editor Export"),
    };
}

template <typename T>
auto spawn_actor(UWorld& world,
                 FString const& label,
                 FTransform const& transform = FTransform::Identity) -> T* {
    auto* const actor{world.SpawnActor<T>(T::StaticClass(), transform)};
    if (IsValid(actor)) {
        actor->SetActorLabel(label, true);
    }
    return actor;
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
}

TEST_CLASS(S7InitialStateExporter, "Sandbox.UnitTests")
{
    TEST_METHOD(CollectsSupportedActorsAndRoundTripsThroughExistingImporter)
    {
        auto* const source_world{FAutomationEditorCommonUtils::CreateNewMap()};
        if (!TestRunner->TestNotNull(TEXT("Source editor world is created"), source_world)) {
            return;
        }

        auto* const player{spawn_actor<ATestSpaceShip>(
            *source_world,
            TEXT("Hero"),
            FTransform{FRotator{1.0, 2.0, 3.0}, FVector{100.125, 200.25, 300.5}})};
        auto* const capital{spawn_actor<ATestCapitalShipProxy>(
            *source_world,
            TEXT("Red Flagship"),
            FTransform{FRotator{0.0, 90.0, 0.0}, FVector{-1000.0, 2000.0, 3000.0}})};
        auto* const turret{spawn_actor<ATestStaticTurretsProxy>(
            *source_world,
            TEXT("Blue Turret"),
            FTransform{FRotator{0.0, -90.0, 0.0}, FVector{4000.0, -5000.0, 6000.0}})};
        if (!TestRunner->TestNotNull(TEXT("Player spawns"), player) ||
            !TestRunner->TestNotNull(TEXT("Capital spawns"), capital) ||
            !TestRunner->TestNotNull(TEXT("Turret spawns"), turret)) {
            return;
        }
        player->set_team(ETestTeam::Blue);
        capital->set_team(ETestTeam::Red);
        turret->set_team(ETestTeam::Blue);

        auto const collected{ml::editor::collect_s7_initial_state(*source_world->GetCurrentLevel(),
                                                                  export_metadata())};
        if (!TestRunner->TestTrue(TEXT("Editor state collects"), collected.has_value())) {
            TestRunner->AddError(collected.error());
            return;
        }
        auto const source{ml::s7::emit_initial_level_source(collected->definition)};
        if (!TestRunner->TestTrue(TEXT("Collected state emits"), source.has_value())) {
            TestRunner->AddError(source.error());
            return;
        }

        ml::s7::FLevelDefinitionReader reader;
        auto const read{reader.read_source(*source)};
        if (!TestRunner->TestTrue(TEXT("Exported state reads"), static_cast<bool>(read))) {
            TestRunner->AddError(read.script_error);
            return;
        }
        auto const import_plan{
            ml::editor::make_s7_initial_state_import_plan(read.definition.GetValue())};
        if (!TestRunner->TestTrue(TEXT("Existing importer accepts source"),
                                  import_plan.has_value())) {
            return;
        }

        auto* const target_world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const source_config{LoadObject<USpaceGameLevelConfig>(
            nullptr,
            TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig"))};
        if (!TestRunner->TestNotNull(TEXT("Target editor world is created"), target_world) ||
            !TestRunner->TestNotNull(TEXT("Runtime config loads"), source_config)) {
            return;
        }
        auto* const config{DuplicateObject<USpaceGameLevelConfig>(source_config, target_world)};
        auto const materialised{ml::editor::materialise_s7_initial_state(
            *target_world->GetCurrentLevel(), *config, *import_plan)};
        if (!TestRunner->TestTrue(TEXT("Exported state materialises"),
                                  static_cast<bool>(materialised))) {
            TestRunner->AddError(materialised.error);
            return;
        }

        auto* const imported_player{find_actor<ATestSpaceShip>(*target_world, TEXT("hero"))};
        auto* const imported_capital{
            find_actor<ATestCapitalShipProxy>(*target_world, TEXT("red-flagship"))};
        auto* const imported_turret{
            find_actor<ATestStaticTurretsProxy>(*target_world, TEXT("blue-turret"))};
        if (!TestRunner->TestNotNull(TEXT("Player imports"), imported_player) ||
            !TestRunner->TestNotNull(TEXT("Capital imports"), imported_capital) ||
            !TestRunner->TestNotNull(TEXT("Turret imports"), imported_turret)) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Player team round trips"), imported_player->get_team(), ETestTeam::Blue);
        TestRunner->TestEqual(
            TEXT("Capital team round trips"), imported_capital->get_team(), ETestTeam::Red);
        TestRunner->TestEqual(
            TEXT("Turret team round trips"), imported_turret->get_team(), ETestTeam::Blue);
        TestRunner->TestTrue(
            TEXT("Capital transform round trips"),
            imported_capital->GetActorTransform().Equals(
                FTransform{FRotator{0.0, 90.0, 0.0}, FVector{-1000.0, 2000.0, 3000.0}}, 0.001));
    }

    TEST_METHOD(PlayerlessExportGetsDeterministicObserverCamera)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const blue{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Blue Flagship"))};
        auto* const red{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Red Flagship"))};
        if (!TestRunner->TestNotNull(TEXT("Blue capital spawns"), blue) ||
            !TestRunner->TestNotNull(TEXT("Red capital spawns"), red)) {
            return;
        }
        blue->set_team(ETestTeam::Blue);
        red->set_team(ETestTeam::Red);
        blue->SetActorLocation(FVector{-200000.0, 0.0, 0.0});
        red->SetActorLocation(FVector{200000.0, 0.0, 0.0});

        auto const collected{
            ml::editor::collect_s7_initial_state(*world->GetCurrentLevel(), export_metadata())};
        if (!TestRunner->TestTrue(TEXT("Playerless state collects"), collected.has_value())) {
            return;
        }
        TestRunner->TestFalse(TEXT("No player is invented"),
                              collected->definition.player_entity_id.is_set());
        if (TestRunner->TestTrue(TEXT("Observer camera is generated"),
                                 collected->definition.camera.IsSet())) {
            auto const& camera{collected->definition.camera.GetValue()};
            TestRunner->TestEqual(
                TEXT("One target per team is generated"), camera.target_entity_ids.Num(), 2);
            TestRunner->TestEqual(
                TEXT("Camera distance frames the layout"), camera.distance, 400000.0);
        }
    }

    TEST_METHOD(ReportsUnsupportedLevelActorsWithoutReportingUnrelatedActors)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const capital{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Capital"))};
        auto* const spinner{spawn_actor<ATestTubeSpinnerProxy>(*world, TEXT("Spinner"))};
        auto* const camera{spawn_actor<ACameraActor>(*world, TEXT("Editor Camera"))};
        if (!TestRunner->TestNotNull(TEXT("Capital spawns"), capital) ||
            !TestRunner->TestNotNull(TEXT("Spinner spawns"), spinner) ||
            !TestRunner->TestNotNull(TEXT("Camera spawns"), camera)) {
            return;
        }

        auto const collected{
            ml::editor::collect_s7_initial_state(*world->GetCurrentLevel(), export_metadata())};
        if (!TestRunner->TestTrue(TEXT("Supported state still collects"), collected.has_value())) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Only supported entity is retained"), collected->definition.entities.num(), 1);
        TestRunner->TestEqual(TEXT("Spinner is reported"),
                              collected->warnings.unsupported_actor_classes.FindRef(
                                  ATestTubeSpinnerProxy::StaticClass()->GetFName()),
                              1);
        TestRunner->TestEqual(TEXT("Unrelated camera is ignored"),
                              collected->warnings.unsupported_actor_classes.Num(),
                              1);
    }

    TEST_METHOD(DerivesReadableUniqueIdsDeterministically)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const first{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Red Capital"))};
        auto* const second{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Red_Capital"))};
        if (!TestRunner->TestNotNull(TEXT("First capital spawns"), first) ||
            !TestRunner->TestNotNull(TEXT("Second capital spawns"), second)) {
            return;
        }
        first->set_team(ETestTeam::Red);
        second->set_team(ETestTeam::Red);

        auto const collected{
            ml::editor::collect_s7_initial_state(*world->GetCurrentLevel(), export_metadata())};
        if (!TestRunner->TestTrue(TEXT("Colliding labels collect"), collected.has_value())) {
            return;
        }

        auto const& ids{collected->definition.entities.ids};
        TestRunner->TestTrue(TEXT("Canonical base id is assigned"),
                             ids.Contains(ml::FLevelEntityId{TEXT("red-capital")}));
        TestRunner->TestTrue(TEXT("Collision receives deterministic suffix"),
                             ids.Contains(ml::FLevelEntityId{TEXT("red-capital-2")}));
    }

    TEST_METHOD(ReportsRepresentationalLossOnSupportedActors)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        auto* const parent{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Parent"))};
        auto* const capital{spawn_actor<ATestCapitalShipProxy>(*world, TEXT("Capital"))};
        if (!TestRunner->TestNotNull(TEXT("Parent spawns"), parent) ||
            !TestRunner->TestNotNull(TEXT("Capital spawns"), capital)) {
            return;
        }
        capital->SetActorScale3D(FVector{2.0, 2.0, 2.0});
        capital->AttachToActor(parent, FAttachmentTransformRules::KeepWorldTransform);
        capital->set_health(500);
        capital->set_spawn_cooldown(3.0f);

        auto const collected{
            ml::editor::collect_s7_initial_state(*world->GetCurrentLevel(), export_metadata())};
        if (!TestRunner->TestTrue(TEXT("Lossy state still collects"), collected.has_value())) {
            return;
        }
        TestRunner->TestEqual(
            TEXT("Scale loss is reported"), collected->warnings.ignored_scale_actor_count, 1);
        TestRunner->TestEqual(TEXT("Attachment flattening is reported"),
                              collected->warnings.flattened_attachment_actor_count,
                              1);
        TestRunner->TestEqual(TEXT("Optional overrides are counted"),
                              collected->warnings.ignored_property_override_count,
                              2);
    }

    TEST_METHOD(RejectsMultiplePlayerActors)
    {
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        spawn_actor<ATestSpaceShip>(*world, TEXT("Player One"));
        spawn_actor<ATestSpaceShip>(*world, TEXT("Player Two"));

        auto const collected{
            ml::editor::collect_s7_initial_state(*world->GetCurrentLevel(), export_metadata())};
        TestRunner->TestFalse(TEXT("Multiple players are rejected"), collected.has_value());
        if (!collected) {
            TestRunner->TestTrue(TEXT("Failure explains player conflict"),
                                 collected.error().Contains(TEXT("only one")));
        }
    }
};

#include <ioj/levels/authoring/level_definition_writer.h>
#include <SandboxEditor/levels/S7EntityGeometryExport.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxShaders/GpuStarfield/GpuStarfieldActor.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <FileHelpers.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformFileManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>

#include <expected>

namespace s7_level_playable_tests {
template <typename Value>
auto assert_success(FAutomationTestBase& test,
                    TCHAR const* description,
                    std::expected<Value, FString> const& result) -> bool {
    if (test.TestTrue(description, result.has_value())) {
        return true;
    }
    test.AddError(result.error());
    return false;
}
}

TEST_CLASS(S7PlayableLevel, "Sandbox.UnitTests")
{
    AFTER_EACH()
    {
        if (GEditor && GEditor->PlayWorld) {
            GEditor->EndPlayMap();
        }
        if (!source_directory_.IsEmpty()) {
            IFileManager::Get().DeleteDirectory(*source_directory_, false, true);
        }
    }

    void play_authored_scene(bool const observer) {
        using s7_level_playable_tests::assert_success;
        using namespace ioj::levels;
        auto* const world{FAutomationEditorCommonUtils::CreateNewMap()};
        if (!TestRunner->TestNotNull(TEXT("Editor world"), world)) {
            return;
        }
        source_directory_ = FPaths::Combine(FPaths::ProjectSavedDir(),
                                            TEXT("Automation"),
                                            TEXT("S7PlayableLevel"),
                                            FGuid::NewGuid().ToString());
        auto const settings{FPaths::Combine(source_directory_, TEXT("Settings"))};
        if (!TestRunner->TestTrue(
                TEXT("Install shared settings and geometry"),
                FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(
                    *settings,
                    *FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts/Settings")),
                    true))) {
            return;
        }
        auto const root{FPaths::Combine(source_directory_, TEXT("Playable/Playable.scm"))};
        auto& editor_level{*world->GetCurrentLevel()};
        auto created{
            ioj::editor::set_up_s7_mission(editor_level, ioj::editor::MissionSetup::Create, root)};
        if (!assert_success(
                *TestRunner, TEXT("Create generated configuration and human root"), created)) {
            return;
        }
        auto* const document{*created};
        auto baseline{ioj::editor::read_s7_editor_input(*document)};
        if (!assert_success(*TestRunner, TEXT("Read generated baseline"), baseline)) {
            return;
        }
        baseline->entities = {
            {.id = EntityId{"enemy"},
             .archetype = EntityArchetype::CapitalShip,
             .team = ioj::Team::Red,
             .position = {30000., 0., 0.},
             .rotation = {10., 370., 20.},
             .spawn_at = ioj::NonNegativeFinite<double>{5.},
             .initial_spawn_delay = ioj::NonNegativeFinite<float>{60.f}},
        };
        if (observer) {
            baseline->entities.push_back({
                .id = EntityId{"ally"},
                .archetype = EntityArchetype::CapitalShip,
                .team = ioj::Team::Blue,
                .position = {-30000., 0., 0.},
                .initial_spawn_delay = ioj::NonNegativeFinite<float>{60.f},
            });
            baseline->camera = LevelCameraDefinition{{EntityId{"ally"}}, {-1., 0., 0.}, 1000.};
        } else {
            baseline->entities.push_back({
                .id = EntityId{"player"},
                .archetype = EntityArchetype::PlayerFighter,
                .team = ioj::Team::Blue,
            });
            baseline->player = EntityId{"player"};
        }
        auto const source{ioj::to_fstring(authoring::emit_editor_level_source(*baseline))};
        if (!TestRunner->TestTrue(TEXT("Write generated records"),
                                  FFileHelper::SaveStringToFile(
                                      source,
                                      *ioj::editor::s7_editor_config_path(*document),
                                      FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) {
            return;
        }
        if (!TestRunner->TestTrue(
                TEXT("Set the mission star scale"),
                FFileHelper::SaveStringToFile(
                    TEXT("\n(set! mission (copy-with mission :settings (settings-layers (level-ref "
                         "mission :settings) "
                         "(settings :starfield (starfield :star (star :scale 2))))))\n"),
                    *root,
                    FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                    &IFileManager::Get(),
                    FILEWRITE_Append))) {
            return;
        }
        auto definition{ioj::editor::read_s7_authored_level(*document)};
        if (!assert_success(*TestRunner, TEXT("Evaluate human entry point"), definition) ||
            !assert_success(*TestRunner,
                            TEXT("Materialize editor proxies"),
                            ioj::editor::import_s7_level(editor_level, *document))) {
            return;
        }
        auto const* enemy_binding{document->entities.FindByPredicate(
            [](auto const& binding) { return binding.id == TEXT("enemy"); })};
        if (!TestRunner->TestNotNull(TEXT("Editor enemy binding"), enemy_binding)) {
            return;
        }
        auto* const editor_enemy{CastChecked<ioj::ATestCapitalShipProxy>(enemy_binding->actor)};
        TestRunner->TestTrue(TEXT("An imported scene matches its saved export"),
                             ioj::editor::s7_scene_needs_export(*document) == false);
        if (!observer) {
            editor_enemy->SetActorLocation(FVector{90000., 0., 0.});
            GEditor->BroadcastEndObjectMovement(*editor_enemy);
            TestRunner->TestTrue(TEXT("Moving an exported actor requires export"),
                                 ioj::editor::s7_scene_needs_export(*document) == true);
            editor_enemy->spawn_time_seconds = 0.;
            world->SpawnActor<ioj::ATestCapitalShipProxy>();
            if (!assert_success(*TestRunner,
                                TEXT("Validate the saved scene with pending edits"),
                                ioj::editor::validate_playable_s7_level(*document))) {
                return;
            }
        } else {
            auto const targets{document->camera.targets};
            document->camera.targets = {editor_enemy};
            FPropertyChangedEvent change{FindFProperty<FProperty>(
                document->GetClass(), GET_MEMBER_NAME_CHECKED(AS7LevelAuthoringDocument, camera))};
            document->PostEditChangeProperty(change);
            TestRunner->TestTrue(TEXT("Changing the observer target requires export"),
                                 ioj::editor::s7_scene_needs_export(*document) == true);
            document->camera.targets = targets;
            for (TActorIterator<ioj::ATestBatchOrchestrator> it{world}; it; ++it) {
                it->set_start_mode(ioj::EOrchestratorStartMode::Automatic);
            }
        }

        TestRunner->AddExpectedMessage(TEXT("Scene edits need export"),
                                       ELogVerbosity::Warning,
                                       EAutomationExpectedMessageFlags::Contains,
                                       1);
        TActorIterator<ioj::AGpuStarfieldActor> preview{world};
        if (!TestRunner->TestTrue(TEXT("Editor starfield exists"), static_cast<bool>(preview))) {
            return;
        }
        auto const preview_radius{
            preview->GetRootComponent()->CalcBounds(FTransform::Identity).SphereRadius};
        TestCommandBuilder.Do([] { FStartPIECommand{false}.Update(); })
            .Until([] { return GEditor->PlayWorld && GEditor->PlayWorld->HasBegunPlay(); },
                   FTimespan::FromSeconds(30))
            .Then([this, observer, preview_radius] {
                auto* const play_world{GEditor->PlayWorld.Get()};
                if (!TestRunner->TestNotNull(TEXT("PIE world"), play_world)) {
                    return;
                }
                TActorIterator<ioj::ATestBatchOrchestrator> it{play_world};
                if (!TestRunner->TestTrue(TEXT("PIE orchestrator"), static_cast<bool>(it))) {
                    return;
                }
                auto& orchestrator{**it};
                if (!TestRunner->TestTrue(TEXT("Authored simulation starts"),
                                          orchestrator.get_state() ==
                                              ioj::sim::LevelState::Running)) {
                    return;
                }
                TActorIterator<ioj::AGpuStarfieldActor> starfield{play_world};
                if (!TestRunner->TestTrue(TEXT("Runtime starfield exists"),
                                          static_cast<bool>(starfield))) {
                    return;
                }
                auto const radius{
                    starfield->GetRootComponent()->CalcBounds(FTransform::Identity).SphereRadius};
                TestRunner->TestTrue(TEXT("Mission scale doubles the starfield bounds"),
                                     FMath::IsNearlyEqual(radius, 2. * preview_radius));
                orchestrator.SetActorTickEnabled(false);
                auto* const controller{Cast<ioj::ASpaceGamePlayerController>(
                    UGameplayStatics::GetPlayerController(play_world, 0))};
                if (!TestRunner->TestNotNull(TEXT("Space-game controller"), controller)) {
                    return;
                }
                TestRunner->TestTrue(
                    TEXT("Runtime control context matches viewpoint"),
                    controller->get_active_control_context() ==
                        (observer ? ControlContext::Observer : ControlContext::Player));
                TestRunner->TestTrue(TEXT("Runtime uses the authored viewpoint"),
                                     observer
                                         ? IsValid(Cast<ACameraActor>(controller->GetViewTarget()))
                                         : controller->GetPawn() == orchestrator.get_player_ship());
                TestRunner->TestFalse(
                    TEXT("PIE proxies do not remain as a second entity source"),
                    static_cast<bool>(TActorIterator<ioj::ATestCapitalShipProxy>{play_world}));

                auto const initial_capitals{observer ? uint32{1} : uint32{0}};
                auto const* simulation{orchestrator.get_level_simulation()};
                if (!TestRunner->TestNotNull(TEXT("Level simulation"), simulation)) {
                    return;
                }
                auto const& clock{simulation->clock()};
                TestRunner->TestEqual(TEXT("Delayed entity is absent at startup"),
                                      simulation->capital_ships().num_instances(),
                                      initial_capitals);
                TestRunner->TestTrue(TEXT("Startup precedes scheduled spawn"),
                                     clock.simulation_time() < 4.);
                orchestrator.tick(FMath::Max(0., 4. - clock.simulation_time()));
                TestRunner->TestEqual(TEXT("Delayed entity remains absent before its time"),
                                      simulation->capital_ships().num_instances(),
                                      initial_capitals);
                orchestrator.tick(1.1);
                auto const& capitals{simulation->capital_ships()};
                if (TestRunner->TestEqual(TEXT("Authored event spawns the delayed entity"),
                                          capitals.num_instances(),
                                          initial_capitals + 1)) {
                    TestRunner->TestEqual(
                        TEXT("Enemy spawns at its saved position"),
                        capitals.get_entities().view_locations().xs()[initial_capitals],
                        30000.f);
                }
            })
            .Then([this, document, editor_enemy, source] {
                GEditor->EndPlayMap();
                if (!TestRunner->TestTrue(TEXT("PIE preserves editor actors"),
                                          IsValid(document) && IsValid(editor_enemy))) {
                    return;
                }
                FString saved;
                TestRunner->TestTrue(TEXT("Play preserves the saved scene"),
                                     FFileHelper::LoadFileToString(
                                         saved, *ioj::editor::s7_editor_config_path(*document)) &&
                                         saved == source);
                auto const* binding{document->entities.FindByPredicate(
                    [](auto const& value) { return value.id == TEXT("enemy"); })};
                TestRunner->TestTrue(TEXT("PIE preserves editor binding identity"),
                                     binding && binding->actor == editor_enemy &&
                                         editor_enemy->export_to_script);
            });
    }

    TEST_METHOD(DelayedObserverScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(true); }
    TEST_METHOD(DelayedPlayerScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(false); }

    TEST_METHOD(RepeatedGeometryExportsMatch)
    {
        source_directory_ = FPaths::Combine(
            FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString());
        IFileManager::Get().MakeDirectory(*source_directory_, true);
        FString previous;
        for (auto const filename : {TEXT("first.scm"), TEXT("second.scm")}) {
            auto const path{FPaths::Combine(source_directory_, filename)};
            if (!s7_level_playable_tests::assert_success(
                    *TestRunner,
                    TEXT("Export configured meshes"),
                    ioj::editor::export_entity_geometry(path))) {
                return;
            }
            FString source;
            if (!TestRunner->TestTrue(TEXT("Read exported module"),
                                      FFileHelper::LoadFileToString(source, *path))) {
                return;
            }
            if (!previous.IsEmpty()) {
                TestRunner->TestEqual(
                    TEXT("Identical assets give identical output"), source, previous);
            }
            previous = MoveTemp(source);
        }
    }

    TEST_METHOD(DefendCapitalStartsWithExportedGeometry)
    {
        using namespace ioj;
        auto const map{FPaths::Combine(
            FPaths::ProjectContentDir(),
            TEXT("Levels/FeatureTests/FT_soa_turrets/test_levels/defend_capital.umap"))};
        if (!TestRunner->TestTrue(TEXT("Open Defend Capital map"),
                                  FEditorFileUtils::LoadMap(map, false, false))) {
            return;
        }
        auto* world{GEditor->GetEditorWorldContext().World()};
        auto document{editor::find_level_authoring_document(*world->GetCurrentLevel())};
        if (!TestRunner->TestTrue(TEXT("Map embeds its project-relative mission"),
                                  document && IsValid(*document) &&
                                      (*document)->source_path ==
                                          TEXT("LevelScripts/defend_capital/defend_capital.scm"))) {
            return;
        }
        auto definition{editor::read_s7_authored_level(**document)};
        if (!s7_level_playable_tests::assert_success(
                *TestRunner, TEXT("Load linked mission"), definition)) {
            return;
        }
        auto const bounds{definition->settings().entity_geometry.player_ship.half_extents};
        TestCommandBuilder.Do([] { FStartPIECommand{false}.Update(); })
            .Until([] { return GEditor->PlayWorld && GEditor->PlayWorld->HasBegunPlay(); },
                   FTimespan::FromSeconds(30))
            .Then([this, bounds] {
                TActorIterator<ATestBatchOrchestrator> it{GEditor->PlayWorld.Get()};
                if (!TestRunner->TestTrue(TEXT("PIE orchestrator"), static_cast<bool>(it))) {
                    return;
                }
                auto* simulation{it->get_level_simulation()};
                if (!TestRunner->TestNotNull(TEXT("Defend Capital simulation"), simulation)) {
                    return;
                }
                TestRunner->TestEqual(TEXT("PIE uses the exported player collision radius"),
                                      simulation->spatial_query_manager().entity_type_radius(
                                          sim::EntityType::PlayerShip),
                                      std::max({bounds.x(), bounds.y(), bounds.z()}));
                it->SetActorTickEnabled(false);
                it->tick(8.0);
                simulation = it->get_level_simulation();
                TestRunner->TestTrue(TEXT("Fighters launch and survive in Play"),
                                     simulation && simulation->fighters().num_instances() > 0);
            });
    }

    FString source_directory_;
};

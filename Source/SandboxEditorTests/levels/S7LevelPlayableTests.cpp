#include <ioj/levels/authoring/level_definition_writer.h>
#include <SandboxEditor/levels/S7LevelAuthoringDocument.h>
#include <SandboxEditor/levels/S7LevelAuthoringSession.h>
#include <SandboxEditor/levels/S7LevelPlayableSetup.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCoreEngine/strings.h>

#include <Camera/CameraActor.h>
#include <CQTest.h>
#include <Editor.h>
#include <Engine/Level.h>
#include <Engine/World.h>
#include <EngineUtils.h>
#include <HAL/FileManager.h>
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
        auto const root{FPaths::Combine(source_directory_, TEXT("Playable.scm"))};
        auto& editor_level{*world->GetCurrentLevel()};
        auto created{ioj::editor::create_level_authoring_document(editor_level, root)};
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
        auto const source{ioj::to_fstring(authoring::emit_level_source(*baseline))};
        if (!TestRunner->TestTrue(TEXT("Write generated records"),
                                  FFileHelper::SaveStringToFile(
                                      source,
                                      *ioj::editor::s7_editor_config_path(*document),
                                      FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) {
            return;
        }
        auto definition{ioj::editor::read_s7_authored_level(*document)};
        if (!assert_success(*TestRunner, TEXT("Evaluate human entry point"), definition) ||
            !assert_success(*TestRunner,
                            TEXT("Materialize editor proxies"),
                            ioj::editor::import_s7_level(editor_level, *document, *definition)) ||
            !assert_success(*TestRunner,
                            TEXT("Set up playable map"),
                            ioj::editor::set_up_playable_s7_level(editor_level, *document))) {
            return;
        }
        auto const* enemy_binding{document->entities.FindByPredicate(
            [](auto const& binding) { return binding.id == TEXT("enemy"); })};
        if (!TestRunner->TestNotNull(TEXT("Editor enemy binding"), enemy_binding)) {
            return;
        }
        auto* const editor_enemy{enemy_binding->actor.Get()};
        if (observer) {
            for (TActorIterator<ioj::ATestBatchOrchestrator> it{world}; it; ++it) {
                it->set_start_mode(ioj::EOrchestratorStartMode::Automatic);
            }
        }

        TestCommandBuilder.Do([] { FStartPIECommand{false}.Update(); })
            .Until([] { return GEditor->PlayWorld && GEditor->PlayWorld->HasBegunPlay(); },
                   FTimespan::FromSeconds(30))
            .Then([this, observer] {
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
                    static_cast<bool>(TActorIterator<ATestCapitalShipProxy>{play_world}));

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
                TestRunner->TestEqual(TEXT("Authored event spawns the delayed entity"),
                                      simulation->capital_ships().num_instances(),
                                      initial_capitals + 1);
            })
            .Then([this, document, editor_enemy] {
                GEditor->EndPlayMap();
                if (!TestRunner->TestTrue(TEXT("PIE preserves editor actors"),
                                          IsValid(document) && IsValid(editor_enemy))) {
                    return;
                }
                auto const* binding{document->entities.FindByPredicate(
                    [](auto const& value) { return value.id == TEXT("enemy"); })};
                TestRunner->TestTrue(TEXT("PIE preserves editor binding identity"),
                                     binding && binding->actor == editor_enemy &&
                                         binding->export_to_script);
            });
    }

    TEST_METHOD(DelayedObserverScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(true); }
    TEST_METHOD(DelayedPlayerScenePlaysThroughAuthoredRuntime)
    { play_authored_scene(false); }

    FString source_directory_;
};

#include "test_level_loader_scenario.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/team.h>
#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestBatchOrchestratorTestAccess.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>

#include <SandboxCoreEngine/actor_utils.h>

#include <Camera/CameraActor.h>
#include <Camera/PlayerCameraManager.h>
#include <Engine/GameViewportClient.h>
#include <EngineUtils.h>
#include <EnhancedInputComponent.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace ioj {
namespace {
auto read_loader_fixture(TCHAR const* filename) -> levels::authoring::LevelDefinitionReadResult {
    auto const path{FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), filename)};
    return levels::authoring::DefinitionReader{}.read_level_file(std::filesystem::path{*path});
}
}

FLevelLoaderScenario::FLevelLoaderScenario(FSimulationTestContext& context,
                                           ELevelLoaderScenario scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {}

void FLevelLoaderScenario::run() {
    TestCommandBuilder.Do([this] {
        initialise_test_driver();
        switch (scenario_) {
            case ELevelLoaderScenario::PlayerMaterialisation:
                load_player_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                check_player_materialisation();
                break;
            case ELevelLoaderScenario::PossessionDuringPause:
                load_player_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                start_simulation();
                check_possession_during_pause();
                break;
            case ELevelLoaderScenario::ObserverMaterialisation:
                load_observer_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                check_observer_materialisation();
                break;
            case ELevelLoaderScenario::ObserverModalTransitions:
                load_observer_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                start_simulation();
                check_observer_modal_transitions();
                break;
            case ELevelLoaderScenario::ObserverBenchmarkSwitching:
                load_observer_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                activate_observer_benchmark();
                break;
            case ELevelLoaderScenario::HeadlessRunControls:
                check_headless_run_controls();
                break;
            case ELevelLoaderScenario::TerminalCompletion:
                load_observer_fixture();
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                start_simulation();
                check_terminal_completion();
                break;
        }
    });
    if (scenario_ == ELevelLoaderScenario::HeadlessRunControls ||
        scenario_ == ELevelLoaderScenario::ObserverBenchmarkSwitching) {
        // CommonUI applies the new input root on its next UI tick.
        TestCommandBuilder
            .Until(
                [this] {
                    if (!checks.all_passed) {
                        return true;
                    }
                    auto const* const controller{CastChecked<ASpaceGamePlayerController>(
                        context_.world.GetFirstPlayerController())};
                    auto const* const viewport{context_.world.GetGameViewport()};
                    return controller->ShouldShowMouseCursor() &&
                           (!IsValid(viewport) ||
                            viewport->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
                },
                FTimespan{0, 0, 2})
            .Then([this] {
                SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                if (scenario_ == ELevelLoaderScenario::ObserverBenchmarkSwitching) {
                    check_observer_benchmark_switching();
                } else {
                    auto const* const controller{CastChecked<ASpaceGamePlayerController>(
                        context_.world.GetFirstPlayerController())};
                    checks.is_true(
                        controller->ShouldShowMouseCursor(),
                        TEXT("Simulation-only run exposes the pointer for the End button"));
                    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
                }
            });
    }
}

void FLevelLoaderScenario::start_simulation() {
    auto& orchestrator{context_.orchestrator};
    orchestrator.start_simulation();
    orchestrator.SetActorTickEnabled(false);
    orchestrator.tick(orchestrator.get_level_simulation()->clock().tick_period());
}

void FLevelLoaderScenario::load_player_fixture() {
    auto const scripted_definition{read_loader_fixture(TEXT("BorderSkirmish.scm"))};
    if (!checks.is_true(static_cast<bool>(scripted_definition),
                        TEXT("Scheme produces a valid native definition"))) {
        return;
    }

    auto const load_result{context_.orchestrator.load_level(scripted_definition.value())};
    if (!checks.is_true(static_cast<bool>(load_result), TEXT("Valid definition loads"))) {
        return;
    }
}

void FLevelLoaderScenario::check_player_materialisation() {
    checks.are_equal(
        1, count_actors<ATestSpaceShip>(context_.world), TEXT("Loader spawns one player"));
    checks.are_equal(0,
                     count_actors<ATestCapitalShipProxy>(context_.world),
                     TEXT("Loader does not create capital proxies"));
    checks.are_equal(0,
                     count_actors<ATestStaticTurretsProxy>(context_.world),
                     TEXT("Loader does not create turret proxies"));

    auto const* const player{context_.orchestrator.get_player_ship()};
    if (checks.is_valid(player, TEXT("Loader binds the player to the orchestrator"))) {
        auto* const player_controller{
            Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
        if (checks.is_valid(player_controller, TEXT("Test world has a player controller"))) {
            checks.is_true(player_controller->GetPawn() == player,
                           TEXT("Player controller possesses the authored player ship"));
            checks.is_true(player_controller->get_active_control_context() ==
                               ::ioj::levels::ControlContext::Player,
                           TEXT("Loading initializes the simulation and activates player input"));
            checks.is_true(Cast<UShipHudWidget>(player_controller->get_active_hud()) != nullptr,
                           TEXT("Loading creates the player HUD"));
        }
        checks.are_equal(
            ETestTeam::Blue, player->get_team(), TEXT("Loader resolves the player team"));
        checks.dist_zero(FVector{0.0, -25000.0, 1000.0},
                         player->GetActorLocation(),
                         0.01,
                         TEXT("Loader applies the player position"));
    }

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_possession_during_pause() {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    if (!checks.is_valid(controller, TEXT("Possession test has a player controller"))) {
        return;
    }
    auto* const ship{Cast<ATestSpaceShip>(controller->GetPawn())};
    if (!checks.is_valid(ship, TEXT("Possession test starts with a possessed ship"))) {
        return;
    }
    auto* const component{CastChecked<UEnhancedInputComponent>(controller->InputComponent)};
    auto const binding_count{component->GetActionEventBindings().Num()};
    auto& orchestrator{context_.orchestrator};
    checks.is_true(controller->get_active_control_context() ==
                       ::ioj::levels::ControlContext::Player,
                   TEXT("Simulation initialization activates player input"));
    checks.is_true(Cast<UShipHudWidget>(controller->get_active_hud()) != nullptr,
                   TEXT("Simulation initialization creates the player HUD"));
    FPlayerControllerTestAccess::toggle_pause(*controller);
    orchestrator.tick(0.0);
    controller->UnPossess();
    checks.is_true(
        orchestrator.get_state() == sim::LevelState::Paused && !IsValid(controller->GetPawn()) &&
            controller->get_active_control_context() == ::ioj::levels::ControlContext::None,
        TEXT("Unpossession during pause keeps gameplay suspended"));
    FPlayerControllerTestAccess::toggle_pause(*controller);
    checks.is_true(orchestrator.get_state() == sim::LevelState::Running &&
                       controller->get_active_control_context() ==
                           ::ioj::levels::ControlContext::None,
                   TEXT("Closing pause without a ship resumes simulation with None"));
    controller->Possess(ship);
    checks.is_true(controller->get_active_control_context() ==
                       ::ioj::levels::ControlContext::Player,
                   TEXT("Possession enables ship control"));

    FPlayerControllerTestAccess::toggle_pause(*controller);
    orchestrator.tick(0.0);
    controller->UnPossess();
    controller->Possess(ship);
    checks.is_true(orchestrator.get_state() == sim::LevelState::Paused &&
                       controller->get_active_control_context() ==
                           ::ioj::levels::ControlContext::None,
                   TEXT("Possession during pause does not enable input"));
    FPlayerControllerTestAccess::toggle_pause(*controller);
    checks.is_true(orchestrator.get_state() == sim::LevelState::Running &&
                       controller->get_active_control_context() ==
                           ::ioj::levels::ControlContext::Player,
                   TEXT("Closing pause restores the newly possessed ship"));
    checks.is_true(component->GetActionEventBindings().Num() == binding_count,
                   TEXT("Possession and modal transitions retain exactly one set of bindings"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::load_observer_fixture() {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    checks.is_valid(controller, TEXT("Observer fixture has a controller"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    // World reset removes transient camera managers while retaining the controller.
    if (!IsValid(controller->PlayerCameraManager)) {
        controller->SpawnPlayerCameraManager();
    }
    checks.is_valid(controller->PlayerCameraManager, TEXT("Observer fixture has a camera manager"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto& orchestrator{context_.orchestrator};
    original_config_.Reset(FTestBatchOrchestratorTestAccess::level_config(orchestrator));
    auto* const resources{duplicate_level_config(*original_config_, orchestrator)};
    checks.is_true(orchestrator.set_level_config(*resources),
                   TEXT("Loader resources are assigned"));
    resources->entity_overlay.soft_target.mesh = nullptr;
    resources->entity_overlay.soft_target.material = nullptr;
    checks.is_true(!resources->is_valid(), TEXT("Unresolved overlay requires missing resources"));

    auto const scripted_definition{read_loader_fixture(TEXT("FleetOverview.scm"))};
    if (!checks.is_true(static_cast<bool>(scripted_definition),
                        TEXT("Playerless Scheme level produces a valid native definition"))) {
        return;
    }

    auto entities{scripted_definition->entities()};
    auto const red_capital{std::ranges::find(
        entities, levels::EntityId{"red-capital"}, &levels::EntitySpawnDefinition::id)};
    check(red_capital != entities.end());
    red_capital->position.x() = 90000.0;
    auto const resolved{levels::LevelDefinition::make(
        {.metadata = scripted_definition->metadata(),
         .camera = scripted_definition->camera(),
         .entities = std::move(entities),
         .settings = settings::Settings{.presentation = {.entity_overlay = {.enabled = false}}}})};
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    check(resolved);
    auto const load_result{context_.orchestrator.load_level(*resolved)};
    if (!checks.is_true(static_cast<bool>(load_result), TEXT("Playerless definition loads"))) {
        return;
    }
    camera_ = Cast<ACameraActor>(controller->GetViewTarget());
    checks.is_valid(camera_.Get(), TEXT("Observer fixture has a camera"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_observer_materialisation() {
    auto& orchestrator{context_.orchestrator};

    checks.are_equal(
        0, count_actors<ATestSpaceShip>(context_.world), TEXT("Loader spawns no player"));
    checks.are_equal(0,
                     count_actors<ATestCapitalShipProxy>(context_.world),
                     TEXT("Loader does not create capital proxies"));
    checks.are_equal(0,
                     count_actors<ATestStaticTurretsProxy>(context_.world),
                     TEXT("Loader does not create turret proxies"));
    checks.is_true(!IsValid(context_.orchestrator.get_player_ship()),
                   TEXT("Orchestrator has no player ship"));

    auto* const player_controller{Cast<ASpaceGamePlayerController>(
        UGameplayStatics::GetPlayerController(&context_.world, 0))};
    checks.is_valid(player_controller, TEXT("Test world has a player controller"));
    auto const expected_focus{FVector{10000.0, 0.0, 0.0}};
    auto const expected_camera_position{expected_focus +
                                        FVector{-1.0, -1.0, 0.6}.GetSafeNormal() * 180000.0};
    ACameraActor* camera{nullptr};
    for (TActorIterator<ACameraActor> it{&context_.world}; it; ++it) {
        if (it->GetActorLocation().Equals(expected_camera_position, 0.1)) {
            camera = *it;
            break;
        }
    }
    camera_ = camera;
    if (checks.is_valid(camera, TEXT("Loader spawns the authored camera"))) {
        auto const camera_position{camera->GetActorLocation()};
        checks.is_true(
            FMath::IsNearlyEqual((camera_position - expected_focus).Size(), 180000.0, 0.1),
            TEXT("Camera uses the authored distance from the target midpoint"));
        auto const direction_to_focus{(expected_focus - camera_position).GetSafeNormal()};
        checks.is_true(camera->GetActorForwardVector().Equals(direction_to_focus, 0.001),
                       TEXT("Camera centres the target midpoint"));
    }
    if (IsValid(player_controller) && IsValid(player_controller->PlayerCameraManager) &&
        IsValid(camera)) {
        checks.is_true(player_controller->GetViewTarget() == camera,
                       TEXT("Player controller uses the authored camera"));
        checks.is_true(player_controller->get_active_control_context() ==
                           ::ioj::levels::ControlContext::Observer,
                       TEXT("Playerless Battle Viewer enters observer context"));
        checks.is_true(player_controller->is_observer_movement_enabled(),
                       TEXT("Observer movement input is enabled"));
        checks.is_true(Cast<UBattleViewerHudWidget>(player_controller->get_active_hud()) != nullptr,
                       TEXT("Observer context creates the Battle Viewer HUD"));
        checks.is_true(Cast<UShipHudWidget>(player_controller->get_active_hud()) == nullptr,
                       TEXT("Observer context does not create the player HUD"));
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_observer_modal_transitions() {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    if (!checks.is_valid(controller, TEXT("Modal test has a player controller"))) {
        return;
    }
    auto* const component{CastChecked<UEnhancedInputComponent>(controller->InputComponent)};
    auto const binding_count{component->GetActionEventBindings().Num()};
    auto const* const hud{controller->get_active_hud()};
    if (!checks.is_true(IsValid(hud), TEXT("Modal test has a HUD"))) {
        return;
    }
    auto const visibility{hud->GetVisibility()};
    auto* const camera{CastChecked<ACameraActor>(camera_.Get())};
    auto const rejects_activation = [&] {
        auto const modal_binding_count{component->GetActionEventBindings().Num()};
        auto* const view_target{controller->GetViewTarget()};
        bool rejected{true};
        for (auto const requested :
             {::ioj::levels::ControlContext::Observer, ::ioj::levels::ControlContext::Benchmark}) {
            rejected &= !controller->activate_playerless_camera(*camera, requested);
            rejected &=
                controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
            rejected &= component->GetActionEventBindings().Num() == modal_binding_count;
            rejected &= controller->GetViewTarget() == view_target;
            rejected &= FPlayerControllerTestAccess::has_modal(*controller);
        }
        return rejected;
    };
    auto& orchestrator{context_.orchestrator};
    FPlayerControllerTestAccess::toggle_pause(*controller);
    checks.is_true(orchestrator.get_state() == sim::LevelState::Running &&
                       !FPlayerControllerTestAccess::has_modal(*controller),
                   TEXT("Pause waits for the next simulation advance before opening its menu"));
    orchestrator.tick(0.0);
    checks.is_true(FPlayerControllerTestAccess::has_modal(*controller),
                   TEXT("Pause opens a modal"));
    checks.is_true(rejects_activation(),
                   TEXT("Pause rejects camera contexts without changing input or UI"));
    checks.is_true(orchestrator.get_state() == sim::LevelState::Paused &&
                       controller->get_active_control_context() ==
                           ::ioj::levels::ControlContext::None,
                   TEXT("Pause suspends simulation and gameplay input"));
    checks.is_true(hud->GetVisibility() == ESlateVisibility::Collapsed,
                   TEXT("Pause hides the HUD"));
    FPlayerControllerTestAccess::toggle_pause(*controller);
    checks.is_true(orchestrator.get_state() == sim::LevelState::Running &&
                       !FPlayerControllerTestAccess::has_modal(*controller) &&
                       controller->is_observer_movement_enabled(),
                   TEXT("Closing pause restores Observer and simulation"));

    checks.is_true(hud->GetVisibility() == visibility,
                   TEXT("Modal close restores prior HUD visibility"));
    checks.is_true(component->GetActionEventBindings().Num() == binding_count,
                   TEXT("Modal transitions do not duplicate global or gameplay bindings"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::activate_observer_benchmark() {
    auto* const player_controller{
        CastChecked<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    auto* const camera{CastChecked<ACameraActor>(camera_.Get())};
    observer_transform_ = camera->GetActorTransform();
    checks.is_true(player_controller->activate_run_controls(),
                   TEXT("Run controls can be activated independently of camera setup"));
    checks.is_true(player_controller->get_active_control_context() ==
                       ::ioj::levels::ControlContext::Benchmark,
                   TEXT("Benchmark context becomes active"));
    auto* const input_component{
        CastChecked<UEnhancedInputComponent>(player_controller->InputComponent)};
    auto const benchmark_binding_count{input_component->GetActionEventBindings().Num()};
    checks.is_true(player_controller->activate_playerless_camera(
                       *camera, ::ioj::levels::ControlContext::Benchmark),
                   TEXT("Already-active benchmark can be requested again"));
    checks.are_equal(benchmark_binding_count,
                     input_component->GetActionEventBindings().Num(),
                     TEXT("Repeated benchmark activation does not duplicate exit bindings"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_observer_benchmark_switching() {
    auto* const player_controller{
        CastChecked<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    auto* const camera{CastChecked<ACameraActor>(camera_.Get())};
    checks.is_true(!player_controller->is_observer_movement_enabled(),
                   TEXT("Benchmark context disables observer movement"));
    checks.is_true(player_controller->get_active_hud() == nullptr,
                   TEXT("Benchmark context removes simulation HUDs"));
    checks.is_true(player_controller->get_benchmark_hud() != nullptr,
                   TEXT("Benchmark context creates the minimal benchmark HUD"));
    checks.is_true(player_controller->get_benchmark_hud()->IsActivated(),
                   TEXT("Benchmark HUD is the active CommonUI input root"));
    checks.is_true(player_controller->ShouldShowMouseCursor(),
                   TEXT("Benchmark context shows the mouse cursor"));
    if (auto* const viewport{context_.world.GetGameViewport()}; IsValid(viewport)) {
        checks.is_true(viewport->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture,
                       TEXT("Benchmark context releases mouse capture for HUD interaction"));
    }
    checks.is_true(camera->GetActorTransform().Equals(observer_transform_),
                   TEXT("Entering benchmark retains the observer camera transform"));

    checks.is_true(player_controller->activate_playerless_camera(
                       *camera, ::ioj::levels::ControlContext::Observer),
                   TEXT("Observer context can be restored for continued development"));
    checks.is_true(player_controller->get_active_control_context() ==
                       ::ioj::levels::ControlContext::Observer,
                   TEXT("Exiting benchmark returns to observer context"));
    checks.is_true(player_controller->is_observer_movement_enabled(),
                   TEXT("Exiting benchmark restores observer movement"));
    checks.is_true(Cast<UBattleViewerHudWidget>(player_controller->get_active_hud()) != nullptr,
                   TEXT("Exiting benchmark restores the Battle Viewer HUD"));
    checks.is_true(camera->GetActorTransform().Equals(observer_transform_),
                   TEXT("Exiting benchmark retains the camera transform"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_headless_run_controls() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto& orchestrator{context_.orchestrator};
    original_config_.Reset(FTestBatchOrchestratorTestAccess::level_config(orchestrator));
    orchestrator.set_presentation_enabled(false);

    auto* const config{duplicate_level_config(
        *FTestBatchOrchestratorTestAccess::level_config(orchestrator), orchestrator)};
    config->classes.player_controller_class = nullptr;
    orchestrator.set_level_config(*config);
    auto const player_level{levels::LevelDefinition::make(
        {.metadata = {.id = levels::LevelId{"headless-player"}, .title = "Headless player"},
         .player_entity_id = levels::EntityId{"player"},
         .entities = {{.id = levels::EntityId{"player"},
                       .archetype = levels::EntityArchetype::PlayerFighter,
                       .team = Team::Blue}}})};
    check(player_level);
    auto const player_result{orchestrator.load_level(*player_level)};
    checks.is_true(static_cast<bool>(player_result), TEXT("Headless loading accepts a player"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(orchestrator.get_level_simulation()->player_ship_simulation() != nullptr,
                   TEXT("Headless loading initializes the native player from class geometry"));
    checks.are_equal(0,
                     count_actors<ATestSpaceShip>(context_.world),
                     TEXT("Native player does not require a pawn"));
    checks.are_equal(0,
                     count_actors<ACameraActor>(context_.world),
                     TEXT("Headless player does not require a camera"));

    orchestrator.reset_for_new_level();
    config->classes.player_ship_class = nullptr;
    orchestrator.set_level_config(*config);
    auto const definition{read_loader_fixture(TEXT("FleetOverview.scm"))};
    checks.is_true(static_cast<bool>(definition), TEXT("Playerless fixture can be reloaded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const result{orchestrator.load_level(*definition)};
    checks.is_true(static_cast<bool>(result),
                   TEXT("Playerless level loads without presentation classes"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(orchestrator.get_level_simulation() && !orchestrator.is_presentation_enabled(),
                   TEXT("Headless loading constructs the simulation without presentation"));
    checks.is_true(orchestrator.get_level_simulation()->player_ship_simulation() == nullptr,
                   TEXT("Playerless battle does not create a dummy player simulation"));
    checks.are_equal(0,
                     count_actors<ACameraActor>(context_.world),
                     TEXT("Headless observer does not require a camera"));
    orchestrator.start_simulation();
    auto* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    if (checks.is_valid(controller, TEXT("Simulation-only UI has a player controller"))) {
        orchestrator.pause_simulation();
        checks.is_true(orchestrator.IsActorTickEnabled(),
                       TEXT("A pause request leaves actor ticking enabled"));
        orchestrator.tick(0.0);
        checks.is_true(controller->activate_run_controls(),
                       TEXT("Simulation-only run controls activate without a camera"));
        checks.is_true(controller->get_benchmark_hud() &&
                           controller->get_benchmark_hud()->IsActivated(),
                       TEXT("Simulation-only run exposes the End HUD"));
        checks.is_true(!orchestrator.get_run_ticks_remaining().IsSet(),
                       TEXT("Untimed run has no countdown"));
        auto* const button{Cast<UMenuButtonWidget>(
            controller->get_benchmark_hud()->GetWidgetFromName(TEXT("pause_resume_button")))};
        if (checks.is_true(IsValid(button), TEXT("Run HUD has a Pause/Resume button"))) {
            checks.are_equal(FString{TEXT("Resume")},
                             button->get_text().ToString(),
                             TEXT("Paused starts offer Resume"));
            button->OnClicked().Broadcast();
            checks.is_true(orchestrator.get_state() == sim::LevelState::Running,
                           TEXT("Resume starts the simulation"));
            checks.are_equal(FString{TEXT("Pause")},
                             button->get_text().ToString(),
                             TEXT("Running simulations offer Pause"));
            button->OnClicked().Broadcast();
            checks.is_true(orchestrator.get_state() == sim::LevelState::Running,
                           TEXT("Pause queues the state change"));
            orchestrator.tick(0.0);
            checks.is_true(orchestrator.get_state() == sim::LevelState::Paused,
                           TEXT("Pause suspends the simulation"));
            checks.is_true(orchestrator.IsActorTickEnabled(),
                           TEXT("Paused simulations keep actor ticking enabled"));
            checks.are_equal(FString{TEXT("Resume")},
                             button->get_text().ToString(),
                             TEXT("Applied pause immediately updates the run controls"));
            checks.is_true(controller->get_benchmark_hud()->IsActivated() &&
                               controller->get_active_control_context() ==
                                   levels::ControlContext::Benchmark,
                           TEXT("Pausing retains run controls without opening a modal"));
        }
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::check_terminal_completion() {
    auto* const controller{
        Cast<ASpaceGamePlayerController>(context_.world.GetFirstPlayerController())};
    if (!checks.is_valid(controller, TEXT("Terminal completion has a player controller"))) {
        return;
    }

    auto& orchestrator{context_.orchestrator};
    FTestBatchOrchestratorTestAccess::stop_run(orchestrator, sim::RunEndReason::LevelCompleted);
    orchestrator.tick(0.0);
    checks.is_true(FPlayerControllerTestAccess::has_modal(*controller),
                   TEXT("Stopped levels can open their completion screen"));
    FPlayerControllerTestAccess::close_completion(*controller);
    checks.is_true(orchestrator.get_level_simulation() == nullptr &&
                       !FPlayerControllerTestAccess::has_modal(*controller),
                   TEXT("Closing completion leaves the simulation released"));
    checks.is_true(controller->get_active_control_context() == levels::ControlContext::None,
                   TEXT("Closing terminal completion does not restore gameplay input"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::on_tear_down() {
    if (camera_.IsValid()) {
        camera_->Destroy();
    }
    if (original_config_.IsValid()) {
        auto& orchestrator{context_.orchestrator};
        orchestrator.reset_for_new_level();
        orchestrator.set_presentation_enabled(true);
        orchestrator.set_level_config(*original_config_);
        original_config_.Reset();
    }
}
}

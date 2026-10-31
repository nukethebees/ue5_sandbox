#include "test_level_loader_scenario.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/team.h>
#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudWidget.h>

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
FLevelLoaderCameraScenario::FLevelLoaderCameraScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FLevelLoaderCameraScenario::load_fixture() {
    initialise_test_driver();
    auto& orchestrator{context_.orchestrator};
    original_config_.Reset(const_cast<USpaceGameLevelConfig*>(orchestrator.get_level_config()));
    auto* const resources{duplicate_level_config(*original_config_, orchestrator)};
    checks.is_true(orchestrator.set_level_config(*resources),
                   TEXT("Loader resources are assigned"));
    resources->presentation.entity_overlay.enabled = true;
    resources->entity_overlay.soft_target.mesh = nullptr;
    resources->entity_overlay.soft_target.material = nullptr;
    checks.is_true(!resources->is_valid(), TEXT("Unresolved overlay requires missing resources"));

    levels::authoring::DefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("FleetOverview.scm"))};
    auto const scripted_definition{reader.read_level_file(std::filesystem::path{*script_path})};
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
    checks.is_true(!orchestrator.get_level_config()->presentation.entity_overlay.enabled,
                   TEXT("Level override disables overlay before resource validation"));

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

        auto const observer_transform{camera->GetActorTransform()};
        checks.is_true(
            player_controller->activate_playerless_camera(*camera,
                                                          ::ioj::levels::ControlContext::Benchmark),
            TEXT("Benchmark context can be selected when activating the playerless camera"));
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
        checks.is_true(camera->GetActorTransform().Equals(observer_transform),
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
        checks.is_true(camera->GetActorTransform().Equals(observer_transform),
                       TEXT("Exiting benchmark retains the camera transform"));
    }

    begin_timed_sampling(
        0.05,
        FSimulationSampleCallback::CreateRaw(this, &FLevelLoaderCameraScenario::sample_runtime));
    context_.orchestrator.start_simulation();
    context_.orchestrator.SetActorTickEnabled(false);
}

void FLevelLoaderCameraScenario::sample_runtime(ATestBatchOrchestrator&) {
    if (!modal_sample_ && test_driver->get_time() > 0.0) {
        sample_modal_transitions();
    }
}

void FLevelLoaderCameraScenario::sample_modal_transitions() {
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
    FModalSample sample;
    auto& orchestrator{context_.orchestrator};
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.pause_open = FPlayerControllerTestAccess::has_modal(*controller);
    sample.pause_rejects_activation = rejects_activation();
    sample.pause_suspended =
        orchestrator.get_state() == sim::LevelState::Paused &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
    sample.hud_hidden = hud->GetVisibility() == ESlateVisibility::Collapsed;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.pause_resumed = orchestrator.get_state() == sim::LevelState::Running &&
                           !FPlayerControllerTestAccess::has_modal(*controller) &&
                           controller->is_observer_movement_enabled();

    FMissionCompletion completion;
    completion.persisted = true;
    completion.state = ETeamOutcome::Victory;
    completion.level_display_name = TEXT("Controller modal test");
    FPlayerControllerTestAccess::complete(*controller, completion);
    sample.completion_open = FPlayerControllerTestAccess::has_modal(*controller);
    sample.completion_rejects_activation = rejects_activation();
    sample.completion_suspended =
        orchestrator.get_state() == sim::LevelState::Paused &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
    FPlayerControllerTestAccess::close_completion(*controller);
    sample.completion_resumed = orchestrator.get_state() == sim::LevelState::Running &&
                                !FPlayerControllerTestAccess::has_modal(*controller) &&
                                controller->is_observer_movement_enabled();
    sample.hud_restored = hud->GetVisibility() == visibility;
    sample.input_restored = component->GetActionEventBindings().Num() == binding_count;
    modal_sample_ = sample;
}

void FLevelLoaderCameraScenario::load_headless_fixture() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto& orchestrator{context_.orchestrator};
    if (camera_.IsValid()) {
        camera_->Destroy();
        camera_.Reset();
    }
    orchestrator.reset_for_new_level();
    orchestrator.set_presentation_enabled(false);

    auto* const config{duplicate_level_config(*orchestrator.get_level_config(), orchestrator)};
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
    orchestrator.start_simulation();
    checks.is_true(orchestrator.get_level_simulation()->get_player_ship_simulation() != nullptr,
                   TEXT("Headless startup initializes the native player from class geometry"));
    checks.are_equal(0,
                     count_actors<ATestSpaceShip>(context_.world),
                     TEXT("Native player does not require a pawn"));
    checks.are_equal(0,
                     count_actors<ACameraActor>(context_.world),
                     TEXT("Headless player does not require a camera"));

    orchestrator.reset_for_new_level();
    config->classes.player_ship_class = nullptr;
    orchestrator.set_level_config(*config);
    levels::authoring::DefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("FleetOverview.scm"))};
    auto const definition{reader.read_level_file(std::filesystem::path{*script_path})};
    checks.is_true(static_cast<bool>(definition), TEXT("Playerless fixture can be reloaded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const result{orchestrator.load_level(*definition)};
    checks.is_true(static_cast<bool>(result),
                   TEXT("Playerless level loads without presentation classes"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    orchestrator.start_simulation();
    checks.is_true(orchestrator.get_level_simulation() && !orchestrator.is_presentation_enabled(),
                   TEXT("Headless startup has no presentation state"));
    checks.is_true(orchestrator.get_level_simulation()->get_player_ship_simulation() == nullptr,
                   TEXT("Playerless battle does not create a dummy player simulation"));
    checks.are_equal(0,
                     count_actors<ACameraActor>(context_.world),
                     TEXT("Headless observer does not require a camera"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderCameraScenario::check_runtime() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(modal_sample_.has_value(), TEXT("Modal transitions were sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& modal{*modal_sample_};
    checks.is_true(modal.pause_open, TEXT("Pause opens a modal"));
    checks.is_true(modal.pause_rejects_activation,
                   TEXT("Pause rejects camera contexts without changing input or UI"));
    checks.is_true(modal.pause_suspended, TEXT("Pause suspends simulation and gameplay input"));
    checks.is_true(modal.hud_hidden, TEXT("Pause hides the HUD"));
    checks.is_true(modal.pause_resumed, TEXT("Closing pause restores Observer and simulation"));
    checks.is_true(modal.completion_open, TEXT("Completion opens a modal"));
    checks.is_true(modal.completion_rejects_activation,
                   TEXT("Completion rejects camera contexts without changing input or UI"));
    checks.is_true(modal.completion_suspended,
                   TEXT("Completion suspends simulation and gameplay input"));
    checks.is_true(modal.completion_resumed,
                   TEXT("Closing completion restores Observer and simulation"));
    checks.is_true(modal.hud_restored, TEXT("Modal close restores prior HUD visibility"));
    checks.is_true(modal.input_restored,
                   TEXT("Modal transitions do not duplicate global or gameplay bindings"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderCameraScenario::on_tear_down() {
    if (camera_.IsValid()) {
        camera_->Destroy();
    }
    if (original_config_.IsValid()) {
        auto& orchestrator{context_.orchestrator};
        orchestrator.reset_for_new_level();
        orchestrator.set_presentation_enabled(true);
        orchestrator.set_level_config(*original_config_);
        orchestrator.prepare_level();
        original_config_.Reset();
    }
}

void FLevelLoaderCameraScenario::run() {
    TestCommandBuilder.Do([this] { load_fixture(); })
        .Until([this] { return advance_until_timeline_finished(); }, timeout)
        .Then([this] { check_runtime(); })
        .Then([this] { load_headless_fixture(); });
}

FLevelLoaderScenario::FLevelLoaderScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FLevelLoaderScenario::load_fixture() {
    initialise_test_driver();

    levels::authoring::DefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("BorderSkirmish.scm"))};
    auto const scripted_definition{reader.read_level_file(std::filesystem::path{*script_path})};
    if (!checks.is_true(static_cast<bool>(scripted_definition),
                        TEXT("Scheme produces a valid native definition"))) {
        return;
    }

    auto const load_result{context_.orchestrator.load_level(scripted_definition.value())};
    if (!checks.is_true(static_cast<bool>(load_result), TEXT("Valid definition loads"))) {
        return;
    }

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
                               ::ioj::levels::ControlContext::None,
                           TEXT("Loader defers player input until simulation initialization"));
            checks.is_true(player_controller->get_active_hud() == nullptr,
                           TEXT("Loader defers the player HUD until simulation initialization"));
        }
        checks.are_equal(
            ETestTeam::Blue, player->get_team(), TEXT("Loader resolves the player team"));
        checks.dist_zero(FVector{0.0, -25000.0, 1000.0},
                         player->GetActorLocation(),
                         0.01,
                         TEXT("Loader applies the player position"));
    }

    begin_timed_sampling(
        0.05, FSimulationSampleCallback::CreateRaw(this, &FLevelLoaderScenario::sample_runtime));
    context_.orchestrator.start_simulation();
    context_.orchestrator.SetActorTickEnabled(false);
}

void FLevelLoaderScenario::sample_runtime(ATestBatchOrchestrator&) {
    if (!control_sample_ && test_driver->get_time() > 0.0) {
        sample_controller_lifecycle();
    }
}

void FLevelLoaderScenario::check_runtime() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(control_sample_.has_value(), TEXT("Controller lifecycle was sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& control{*control_sample_};
    checks.is_true(control.input_activated_after_initialisation,
                   TEXT("Simulation initialization activates player input"));
    checks.is_true(control.hud_created_after_initialisation,
                   TEXT("Simulation initialization creates the player HUD"));
    checks.is_true(control.unpossessed_while_paused,
                   TEXT("Unpossession during pause keeps gameplay suspended"));
    checks.is_true(control.resumed_without_ship,
                   TEXT("Closing pause without a ship resumes simulation with None"));
    checks.is_true(control.possession_enabled_ship, TEXT("Possession enables ship control"));
    checks.is_true(control.possession_stayed_suspended,
                   TEXT("Possession during pause does not enable input"));
    checks.is_true(control.resumed_with_ship,
                   TEXT("Closing pause restores the newly possessed ship"));
    checks.is_true(control.bindings_restored,
                   TEXT("Possession and modal transitions retain exactly one set of bindings"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::run() {
    TestCommandBuilder.Do([this] { load_fixture(); })
        .Until([this] { return advance_until_timeline_finished(); }, timeout)
        .Then([this] { check_runtime(); });
}

void FLevelLoaderScenario::sample_controller_lifecycle() {
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
    FControlLifecycleSample sample;
    sample.input_activated_after_initialisation =
        controller->get_active_control_context() == ::ioj::levels::ControlContext::Player;
    sample.hud_created_after_initialisation =
        Cast<UShipHudWidget>(controller->get_active_hud()) != nullptr;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    controller->UnPossess();
    sample.unpossessed_while_paused =
        orchestrator.get_state() == sim::LevelState::Paused && !IsValid(controller->GetPawn()) &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.resumed_without_ship =
        orchestrator.get_state() == sim::LevelState::Running &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
    controller->Possess(ship);
    sample.possession_enabled_ship =
        controller->get_active_control_context() == ::ioj::levels::ControlContext::Player;

    FPlayerControllerTestAccess::toggle_pause(*controller);
    controller->UnPossess();
    controller->Possess(ship);
    sample.possession_stayed_suspended =
        orchestrator.get_state() == sim::LevelState::Paused &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::None;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.resumed_with_ship =
        orchestrator.get_state() == sim::LevelState::Running &&
        controller->get_active_control_context() == ::ioj::levels::ControlContext::Player;
    sample.bindings_restored = component->GetActionEventBindings().Num() == binding_count;
    control_sample_ = sample;
}
}

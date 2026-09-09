#include "test_level_loader_scenario.h"

#include <SandboxTests/support/PlayerControllerTestAccess.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/presentation/widgets/BattleViewerHudWidget.h>
#include <SpaceGame/presentation/widgets/BenchmarkHudWidget.h>
#include <SpaceGame/presentation/widgets/ShipHudWidget.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Camera/CameraActor.h>
#include <Camera/PlayerCameraManager.h>
#include <Engine/GameViewportClient.h>
#include <EngineUtils.h>
#include <EnhancedInputComponent.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/Paths.h>

#include <utility>

namespace ml {
FLevelLoaderCameraScenario::FLevelLoaderCameraScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FLevelLoaderCameraScenario::load_fixture() {
    initialise_test_driver();

    s7::FLevelDefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("FleetOverview.scm"))};
    auto const scripted_definition{reader.read_file(script_path)};
    if (!checks.is_true(static_cast<bool>(scripted_definition),
                        TEXT("Playerless Scheme level produces a valid native definition"))) {
        return;
    }

    auto definition{scripted_definition.definition.GetValue()};
    auto const red_capital_index{
        definition.entities.ids.IndexOfByKey(FLevelEntityId{FName{TEXT("red-capital")}})};
    check(red_capital_index != INDEX_NONE);
    definition.entities.positions.xs[red_capital_index] = 90000.0;

    FLevelLoader loader{context_.orchestrator};
    auto const load_result{loader.load(definition)};
    if (!checks.is_true(static_cast<bool>(load_result), TEXT("Playerless definition loads"))) {
        return;
    }

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
                           EPlayerControlContext::Observer,
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
                                                          EPlayerControlContext::Benchmark),
            TEXT("Benchmark context can be selected when activating the playerless camera"));
        checks.is_true(player_controller->get_active_control_context() ==
                           EPlayerControlContext::Benchmark,
                       TEXT("Benchmark context becomes active"));
        auto* const input_component{
            CastChecked<UEnhancedInputComponent>(player_controller->InputComponent)};
        auto const benchmark_binding_count{input_component->GetActionEventBindings().Num()};
        checks.is_true(player_controller->activate_playerless_camera(
                           *camera, EPlayerControlContext::Benchmark),
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

        checks.is_true(
            player_controller->activate_playerless_camera(*camera, EPlayerControlContext::Observer),
            TEXT("Observer context can be restored for continued development"));
        checks.is_true(player_controller->get_active_control_context() ==
                           EPlayerControlContext::Observer,
                       TEXT("Exiting benchmark returns to observer context"));
        checks.is_true(player_controller->is_observer_movement_enabled(),
                       TEXT("Exiting benchmark restores observer movement"));
        checks.is_true(Cast<UBattleViewerHudWidget>(player_controller->get_active_hud()) != nullptr,
                       TEXT("Exiting benchmark restores the Battle Viewer HUD"));
        checks.is_true(camera->GetActorTransform().Equals(observer_transform),
                       TEXT("Exiting benchmark retains the camera transform"));
    }

    reset_and_reserve_time_series(context_.orchestrator, 0.05, entity_counts_);
    context_.orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLevelLoaderCameraScenario::sample_runtime));
    test_driver->timeline.finish_at(0.05);
    context_.orchestrator.start_simulation();
}

void FLevelLoaderCameraScenario::sample_runtime(ATestBatchOrchestrator& orchestrator) {
    if (modal_samples_.num() == 0 && test_driver->get_time() > 0.0) {
        sample_modal_transitions();
    }
    auto const counts{orchestrator.get_entity_registry().count_alive_per_team_and_type()};
    auto const blue{std::to_underlying(ETestTeam::Blue)};
    auto const red{std::to_underlying(ETestTeam::Red)};
    auto const capital{std::to_underlying(ETestEntityType::CapitalShip)};
    auto const turret{std::to_underlying(ETestEntityType::Turret)};
    entity_counts_.add(test_driver->get_time(),
                       counts[blue][capital] + counts[blue][turret] + counts[red][capital] +
                           counts[red][turret]);
    test_driver->advance_timeline();
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
             {EPlayerControlContext::Observer, EPlayerControlContext::Benchmark}) {
            rejected &= !controller->activate_playerless_camera(*camera, requested);
            rejected &= controller->get_active_control_context() == EPlayerControlContext::None;
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
        orchestrator.get_state() == EOrchestratorState::Paused &&
        controller->get_active_control_context() == EPlayerControlContext::None;
    sample.hud_hidden = hud->GetVisibility() == ESlateVisibility::Collapsed;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.pause_resumed = orchestrator.get_state() == EOrchestratorState::Running &&
                           !FPlayerControllerTestAccess::has_modal(*controller) &&
                           controller->is_observer_movement_enabled();

    FTestMissionCompletion completion;
    completion.persisted = true;
    completion.state = ETestMissionState::Succeeded;
    completion.level_display_name = TEXT("Controller modal test");
    FPlayerControllerTestAccess::complete(*controller, completion);
    sample.completion_open = FPlayerControllerTestAccess::has_modal(*controller);
    sample.completion_rejects_activation = rejects_activation();
    sample.completion_suspended =
        orchestrator.get_state() == EOrchestratorState::Paused &&
        controller->get_active_control_context() == EPlayerControlContext::None;
    FPlayerControllerTestAccess::close_completion(*controller);
    sample.completion_resumed = orchestrator.get_state() == EOrchestratorState::Running &&
                                !FPlayerControllerTestAccess::has_modal(*controller) &&
                                controller->is_observer_movement_enabled();
    sample.hud_restored = hud->GetVisibility() == visibility;
    sample.input_restored = component->GetActionEventBindings().Num() == binding_count;
    modal_samples_.add(test_driver->get_time(), sample);
}

void FLevelLoaderCameraScenario::load_headless_fixture() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto& orchestrator{context_.orchestrator};
    original_config_.Reset(const_cast<USpaceGameLevelConfig*>(orchestrator.get_level_config()));
    orchestrator.reset_for_new_level();
    orchestrator.set_presentation_enabled(false);
    initialise_test_driver();

    auto* const config{
        DuplicateObject<USpaceGameLevelConfig>(orchestrator.get_level_config(), &orchestrator)};
    config->classes.player_controller_class = nullptr;
    config->classes.player_ship_class = nullptr;
    orchestrator.set_level_config(*config);

    FLevelLoader loader{orchestrator};
    auto const player_result{loader.load(example_levels::make_native_example())};
    checks.is_true(!static_cast<bool>(player_result),
                   TEXT("Headless loading rejects player levels"));
    checks.are_equal(0,
                     count_actors<ATestSpaceShip>(context_.world),
                     TEXT("Rejected headless load creates no player"));

    s7::FLevelDefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("FleetOverview.scm"))};
    auto const definition{reader.read_file(script_path)};
    checks.is_true(static_cast<bool>(definition), TEXT("Playerless fixture can be reloaded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const result{loader.load(definition.definition.GetValue())};
    checks.is_true(static_cast<bool>(result),
                   TEXT("Playerless level loads without presentation classes"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.are_equal(0,
                     count_actors<ACameraActor>(context_.world),
                     TEXT("Headless loader does not spawn a camera"));

    reset_and_reserve_time_series(orchestrator, 0.05, entity_counts_);
    orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLevelLoaderCameraScenario::sample_runtime));
    test_driver->timeline.finish_at(0.05);
    orchestrator.start_simulation();
    checks.is_true(orchestrator.get_level_simulation() &&
                       !orchestrator.get_level_simulation()->has_presentation(),
                   TEXT("Headless startup has no presentation state"));
    checks.is_true(orchestrator.get_player_ship_simulation() == nullptr,
                   TEXT("Playerless battle does not create a dummy player simulation"));
}

void FLevelLoaderCameraScenario::check_runtime() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(!entity_counts_.is_empty(), TEXT("Runtime state was sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.are_equal(4,
                     entity_counts_.last_value(),
                     TEXT("All playerless authored entities reach the registry"));
    checks.is_true(!modal_samples_.is_empty(), TEXT("Modal transitions were sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& modal{modal_samples_.last_value()};
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
        .Until(
            [this] {
                return !checks.all_passed ||
                       (test_driver.IsSet() && test_driver->timeline.is_finished());
            },
            timeout)
        .Then([this] { check_runtime(); })
        .Then([this] { load_headless_fixture(); })
        .Until(
            [this] {
                return !checks.all_passed ||
                       (test_driver.IsSet() && test_driver->timeline.is_finished());
            },
            timeout)
        .Then([this] { check_runtime(); });
}

FLevelLoaderScenario::FLevelLoaderScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {}

void FLevelLoaderScenario::load_fixture() {
    initialise_test_driver();
    FLevelLoader loader{context_.orchestrator};

    auto invalid_definition{example_levels::make_native_example()};
    invalid_definition.entities.teams[1] = level_teams::green;
    auto const invalid_result{loader.load(invalid_definition)};
    checks.is_true(!static_cast<bool>(invalid_result), TEXT("Invalid definition is rejected"));
    checks.are_equal(
        0, count_actors<ATestSpaceShip>(context_.world), TEXT("Rejected load spawns no player"));
    checks.are_equal(0,
                     count_actors<ATestCapitalShipProxy>(context_.world),
                     TEXT("Rejected load spawns no capitals"));
    checks.are_equal(0,
                     count_actors<ATestStaticTurretsProxy>(context_.world),
                     TEXT("Rejected load spawns no turrets"));
    checks.is_true(context_.orchestrator.get_state() == EOrchestratorState::Uninitialised,
                   TEXT("Rejected load leaves orchestrator uninitialised"));

    s7::FLevelDefinitionReader reader;
    auto const script_path{
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("BorderSkirmish.scm"))};
    auto const scripted_definition{reader.read_file(script_path)};
    if (!checks.is_true(static_cast<bool>(scripted_definition),
                        TEXT("Scheme produces a valid native definition"))) {
        return;
    }

    auto const load_result{loader.load(scripted_definition.definition.GetValue())};
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
                               EPlayerControlContext::None,
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

    reset_and_reserve_time_series(context_.orchestrator, 0.05, samples);
    context_.orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLevelLoaderScenario::sample_runtime));
    test_driver->timeline.finish_at(0.05);
    context_.orchestrator.start_simulation();
}

void FLevelLoaderScenario::sample_runtime(ATestBatchOrchestrator& orchestrator) {
    if (control_samples_.is_empty() && test_driver->get_time() > 0.0) {
        sample_controller_lifecycle();
    }
    auto const& registry{orchestrator.get_entity_registry()};
    auto const& mission{orchestrator.get_mission_manager()};
    auto const counts{registry.count_alive_per_team_and_type()};
    auto const blue{std::to_underlying(ETestTeam::Blue)};
    auto const red{std::to_underlying(ETestTeam::Red)};
    auto const player_type{std::to_underlying(ETestEntityType::PlayerShip)};
    auto const capital_type{std::to_underlying(ETestEntityType::CapitalShip)};
    auto const turret_type{std::to_underlying(ETestEntityType::Turret)};

    FSample sample{
        .authored_entities = counts[blue][player_type] + counts[blue][capital_type] +
                             counts[red][capital_type] + counts[red][turret_type],
        .blue_players = counts[blue][player_type],
        .blue_capitals = counts[blue][capital_type],
        .red_capitals = counts[red][capital_type],
        .red_turrets = counts[red][turret_type],
        .mission_mode = mission.get_mission_mode(),
        .mission_state = mission.get_mission_state(),
        .mission_kill_target = mission.get_kill_target(),
        .mission_heroes = mission.get_hero_entity_handles().Num(),
        .mission_survivors = mission.get_entity_handles_that_must_survive().Num(),
        .mission_required_kills = mission.get_entity_handles_required_to_kill().Num(),
        .mission_level_name = mission.get_level_id(),
        .mission_level_display_name = mission.get_level_display_name(),
        .saves_mission_results = mission.should_save_mission_results(),
    };
    auto const& entity_data{registry.get_entity_data()};
    auto const entity_count{entity_data.teams.Num()};
    for (int32 i{0}; i < entity_count; ++i) {
        auto const position{get_vector3f(entity_data.locations, i)};
        auto const team{entity_data.teams[i]};
        auto const type{entity_data.entity_types[i]};
        if (type == ETestEntityType::CapitalShip && team == ETestTeam::Blue) {
            sample.blue_capital_position = position;
        } else if (type == ETestEntityType::CapitalShip && team == ETestTeam::Red) {
            sample.red_capital_position = position;
        } else if (type == ETestEntityType::Turret && team == ETestTeam::Red) {
            sample.red_turret_position = position;
        }
    }

    samples.add(test_driver->get_time(), MoveTemp(sample));
    test_driver->advance_timeline();
}

void FLevelLoaderScenario::check_runtime() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(!samples.is_empty(), TEXT("Runtime state was sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& sample{samples.last_value()};
    checks.is_true(!control_samples_.is_empty(), TEXT("Controller lifecycle was sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    auto const& control{control_samples_.last_value()};
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
    checks.are_equal(4, sample.authored_entities, TEXT("All authored entities reach the registry"));
    checks.are_equal(1, sample.blue_players, TEXT("Registry contains the blue player"));
    checks.are_equal(1, sample.blue_capitals, TEXT("Registry contains the blue capital"));
    checks.are_equal(1, sample.red_capitals, TEXT("Registry contains the red capital"));
    checks.are_equal(1, sample.red_turrets, TEXT("Registry contains the red turret"));
    checks.are_equal(ETestMissionMode::KillEnemies,
                     sample.mission_mode,
                     TEXT("Loader configures the authored mission mode"));
    checks.are_equal(
        ETestMissionState::Running, sample.mission_state, TEXT("Authored mission starts running"));
    checks.are_equal(2,
                     sample.mission_kill_target,
                     TEXT("Omitted kill count resolves to the initial enemy population"));
    checks.are_equal(2, sample.mission_heroes, TEXT("Loader resolves authored hero entities"));
    checks.are_equal(1, sample.mission_survivors, TEXT("Loader resolves the protected entity"));
    checks.are_equal(
        1, sample.mission_required_kills, TEXT("Loader resolves the required kill entity"));
    checks.is_true(sample.saves_mission_results,
                   TEXT("Authored missions save against their authored level id"));
    checks.is_true(sample.mission_level_name == FName{TEXT("border-skirmish")},
                   TEXT("Mission results use the authored level id"));
    checks.are_equal(FString{TEXT("Border Skirmish")},
                     sample.mission_level_display_name,
                     TEXT("Mission UI uses the authored display title"));
    checks.dist_zero(FVector3f{-40000.f, 0.f, 0.f},
                     sample.blue_capital_position,
                     0.01f,
                     TEXT("Blue capital runtime position matches the definition"));
    checks.dist_zero(FVector3f{40000.f, 0.f, 0.f},
                     sample.red_capital_position,
                     0.01f,
                     TEXT("Red capital runtime position matches the definition"));
    checks.dist_zero(FVector3f{30000.f, 15000.f, 0.f},
                     sample.red_turret_position,
                     0.01f,
                     TEXT("Red turret runtime position matches the definition"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderScenario::run() {
    TestCommandBuilder.Do([this] { load_fixture(); })
        .Until(
            [this] {
                return !checks.all_passed ||
                       (test_driver.IsSet() && test_driver->timeline.is_finished());
            },
            timeout)
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
        controller->get_active_control_context() == EPlayerControlContext::Player;
    sample.hud_created_after_initialisation =
        Cast<UShipHudWidget>(controller->get_active_hud()) != nullptr;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    controller->UnPossess();
    sample.unpossessed_while_paused =
        orchestrator.get_state() == EOrchestratorState::Paused && !IsValid(controller->GetPawn()) &&
        controller->get_active_control_context() == EPlayerControlContext::None;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.resumed_without_ship =
        orchestrator.get_state() == EOrchestratorState::Running &&
        controller->get_active_control_context() == EPlayerControlContext::None;
    controller->Possess(ship);
    sample.possession_enabled_ship =
        controller->get_active_control_context() == EPlayerControlContext::Player;

    FPlayerControllerTestAccess::toggle_pause(*controller);
    controller->UnPossess();
    controller->Possess(ship);
    sample.possession_stayed_suspended =
        orchestrator.get_state() == EOrchestratorState::Paused &&
        controller->get_active_control_context() == EPlayerControlContext::None;
    FPlayerControllerTestAccess::toggle_pause(*controller);
    sample.resumed_with_ship =
        orchestrator.get_state() == EOrchestratorState::Running &&
        controller->get_active_control_context() == EPlayerControlContext::Player;
    sample.bindings_restored = component->GetActionEventBindings().Num() == binding_count;
    control_samples_.add(test_driver->get_time(), sample);
}
}

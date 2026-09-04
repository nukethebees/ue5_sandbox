#include "test_level_loader_scenario.h"

#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/levels/ExampleLevels.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Camera/CameraActor.h>
#include <Camera/PlayerCameraManager.h>
#include <EngineUtils.h>
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
    checks.are_equal(2,
                     count_actors<ATestCapitalShipProxy>(context_.world),
                     TEXT("Loader spawns two capital proxies"));
    checks.are_equal(2,
                     count_actors<ATestStaticTurretsProxy>(context_.world),
                     TEXT("Loader spawns two turret proxies"));
    checks.is_true(!IsValid(context_.orchestrator.get_player_ship()),
                   TEXT("Orchestrator has no player ship"));

    auto* const player_controller{UGameplayStatics::GetPlayerController(&context_.world, 0)};
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
    }

    reset_and_reserve_time_series(context_.orchestrator, 0.05, entity_counts_);
    context_.orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLevelLoaderCameraScenario::sample_runtime));
    test_driver->timeline.finish_at(0.05);
    context_.orchestrator.start_simulation();
}

void FLevelLoaderCameraScenario::sample_runtime(ATestBatchOrchestrator& orchestrator) {
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

void FLevelLoaderCameraScenario::check_runtime() {
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.is_true(!entity_counts_.is_empty(), TEXT("Runtime state was sampled"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    checks.are_equal(4,
                     entity_counts_.last_value(),
                     TEXT("All playerless authored entities reach the registry"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLevelLoaderCameraScenario::on_tear_down() {
    if (camera_.IsValid()) {
        camera_->Destroy();
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
    checks.are_equal(2,
                     count_actors<ATestCapitalShipProxy>(context_.world),
                     TEXT("Loader spawns two capital proxies"));
    checks.are_equal(1,
                     count_actors<ATestStaticTurretsProxy>(context_.world),
                     TEXT("Loader spawns one turret proxy"));

    auto const* const player{context_.orchestrator.get_player_ship()};
    if (checks.is_valid(player, TEXT("Loader binds the player to the orchestrator"))) {
        auto* const player_controller{context_.world.GetFirstPlayerController()};
        if (checks.is_valid(player_controller, TEXT("Test world has a player controller"))) {
            checks.is_true(player_controller->GetPawn() == player,
                           TEXT("Player controller possesses the authored player ship"));
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
        .mission_level_name = mission.get_level_name(),
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
}

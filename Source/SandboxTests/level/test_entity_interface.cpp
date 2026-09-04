#include "test_entity_interface_scenario.h"

#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
FEntityInterfaceScenario::FEntityInterfaceScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    level_config = DuplicateObject<USpaceGameLevelConfig>(&context.config, &context.orchestrator);
    if (!checks.not_nullptr(level_config.Get(), TEXT("Entity interface level config is created"))) {
        return;
    }
    level_config->collision_grid.grid_size.Z = 2000000.f;
    context.orchestrator.set_level_config(*level_config);

    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FEntityInterfaceScenario::on_tear_down() {
    context_.orchestrator.set_level_config(const_cast<USpaceGameLevelConfig&>(context_.config));
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FEntityInterfaceScenario::spawn_fixture() {
    auto* const capital_actor{
        const_cast<ATestCapitalShips*>(context_.orchestrator.get_capital_ships_actor())};
    if (!checks.is_valid(capital_actor, TEXT("Capital batch actor is available"))) {
        return;
    }

    auto* const capital_config{duplicate_capital_ships_config(context_.config, *capital_actor)};
    if (!checks.not_nullptr(capital_config, TEXT("Entity interface capital config is created"))) {
        return;
    }
    capital_config->spawn_delay = 10.f;
    capital_config->max_health = 10000;
    capital_config->visual_logger_style = nullptr;
    capital_actor->set_actor_config(capital_config);

    auto* const player{spawn_player_ship(
        context_.world, context_.config.classes.player_ship_class, &context_.config.player_ship)};
    if (!checks.is_valid(player, TEXT("Player ship is spawned"))) {
        return;
    }
    player->SetActorTransform(
        FTransform{FRotator{0.f, 50.f, 0.f}, FVector{-2970.f, 22120.f, 10800.f}});
    context_.orchestrator.set_player_ship(*player);

    ATestStaticTurretsProxy* turret{nullptr};
    spawn_actors<ATestStaticTurretsProxy, 1>(
        context_.world, [&](ATestStaticTurretsProxy& actor, int32, ESpawnPhase const phase) {
            if (phase == ESpawnPhase::PreSpawn) {
                actor.set_actor_config(&context_.config.turrets);
                actor.set_team(ETestTeam::White);
                return;
            }
            actor.SetActorLocation(FVector{118176.f, 23320.f, 15650.f});
            turret = &actor;
        });
    if (!checks.is_valid(turret, TEXT("Turret proxy is spawned"))) {
        return;
    }

    auto* const green{spawn_capital_proxy(context_.world,
                                          context_.config,
                                          checks,
                                          TEXT("green_capital"),
                                          FVector{-16970.f, -690.f, 4360.f})};
    auto* const red{spawn_capital_proxy(context_.world,
                                        context_.config,
                                        checks,
                                        TEXT("red_capital"),
                                        FVector{35140.f, 2170.f, 4360.f})};
    auto* const player_targeter{spawn_capital_proxy(context_.world,
                                                    context_.config,
                                                    checks,
                                                    TEXT("player_targeter"),
                                                    FVector{-18030.f, 18790.f, 4360.f})};
    auto* const turret_targeter{spawn_capital_proxy(context_.world,
                                                    context_.config,
                                                    checks,
                                                    TEXT("turret_targeter"),
                                                    FVector{-18290.f, 39160.f, 4360.f})};
    if (!checks.is_valid(green, TEXT("Green capital is spawned")) ||
        !checks.is_valid(red, TEXT("Red capital is spawned")) ||
        !checks.is_valid(player_targeter, TEXT("Player-targeting capital is spawned")) ||
        !checks.is_valid(turret_targeter, TEXT("Turret-targeting capital is spawned"))) {
        return;
    }
    green->set_team(ETestTeam::Green);
    green->set_actor_config(capital_config);
    green->set_target_ship(red);
    red->set_team(ETestTeam::Red);
    red->set_actor_config(capital_config);
    red->set_target_ship(green);
    player_targeter->set_team(ETestTeam::Green);
    player_targeter->set_actor_config(capital_config);
    player_targeter->set_target_ship(player);
    turret_targeter->set_team(ETestTeam::Green);
    turret_targeter->set_actor_config(capital_config);
    turret_targeter->set_target_ship(turret);
}

void FEntityInterfaceScenario::initial_setup() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    reset_and_reserve_time_series(test_driver->orchestrator,
                                  test_time,
                                  capital_proxy_counts,
                                  turret_proxy_counts,
                                  spinner_proxy_counts,
                                  capital_target_handles,
                                  capital_target_alive);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FEntityInterfaceScenario::on_end_tick));
    test_driver->timeline.finish_at(test_time);
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FEntityInterfaceScenario::sample_values() {
    auto const time{test_driver->get_time()};
    capital_proxy_counts.add(time, get_actors<ATestCapitalShipProxy>(context_.world).Num());
    turret_proxy_counts.add(time, get_actors<ATestStaticTurretsProxy>(context_.world).Num());
    spinner_proxy_counts.add(time, get_actors<ATestTubeSpinnerProxy>(context_.world).Num());

    auto const* const capitals{test_driver->orchestrator.get_capital_ships()};
    TArray<FRegistryEntityHandle> target_handles;
    target_handles.Append(capitals->get_target_handles());
    capital_target_handles.add(time, MoveTemp(target_handles));

    TArray<uint8> target_alive;
    auto const& registry{test_driver->orchestrator.get_entity_registry()};
    for (auto const handle : capitals->get_target_handles()) {
        target_alive.Add(registry.is_valid_alive(handle));
    }
    capital_target_alive.add(time, MoveTemp(target_alive));
}

void FEntityInterfaceScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FEntityInterfaceScenario::check_no_proxies_alive(int32 const sample_index) {
    checks.are_equal(
        0, capital_proxy_counts.value_at(sample_index), TEXT("ATestCapitalShipProxy check"));
    checks.are_equal(
        0, turret_proxy_counts.value_at(sample_index), TEXT("ATestStaticTurretsProxy check"));
    checks.are_equal(
        0, spinner_proxy_counts.value_at(sample_index), TEXT("ATestTubeSpinnerProxy check"));
}

void FEntityInterfaceScenario::check_capital_targets(int32 const sample_index) {
    auto const& handles{capital_target_handles.value_at(sample_index)};
    auto const& alive{capital_target_alive.value_at(sample_index)};
    for (int32 i{0}; i < handles.Num(); ++i) {
        checks.is_true(alive[i] != 0, FString::Printf(TEXT("Target check: %d"), i));
    }
}

void FEntityInterfaceScenario::main_checks() {
    auto const sample_index{capital_target_handles.nearest_index(test_time)};
    check_no_proxies_alive(sample_index);
    check_capital_targets(sample_index);
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FEntityInterfaceScenario::run() {
    run_until_timeline_finished(
        [this] { initial_setup(); }, FTimespan{0, 0, 1}, [this] { main_checks(); });
}
}

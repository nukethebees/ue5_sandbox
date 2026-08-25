#include "test_entity_registry_scenario.h"

#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/simulation/TestSimulationConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SandboxGameShared/utilities/enums.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/array_math.h>

namespace ml {
namespace {
constexpr TStaticArray<int32, 6> expected_team_counts{0, 1, 2, 3, 4, 5};
}

FEntityRegistryScenario::FEntityRegistryScenario(FSimulationTestContext& context,
                                                 EEntityRegistryScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    switch (scenario_) {
        case EEntityRegistryScenario::TeamCounts:
            expected_kills = 0;
            break;
        case EEntityRegistryScenario::OnePlayerKill:
            expected_kills = 1;
            break;
        case EEntityRegistryScenario::TwoPlayerKills:
            expected_kills = 2;
            break;
    }
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

/* ------------------------------------------------------------------------------------------ */
// Fixture
/* ------------------------------------------------------------------------------------------ */
void FEntityRegistryScenario::spawn_fixture() {
    auto* const capital_actor{
        const_cast<ATestCapitalShips*>(context_.orchestrator.get_capital_ships())};
    if (!checks.is_valid(capital_actor, TEXT("Capital batch actor is available"))) {
        return;
    }

    auto* const capital_config{duplicate_capital_ships_config(context_.config, *capital_actor)};
    if (!checks.not_nullptr(capital_config, TEXT("Entity registry capital config is created"))) {
        return;
    }
    capital_config->visual_logger_style = nullptr;
    capital_actor->set_actor_config(capital_config);

    if (scenario_ != EEntityRegistryScenario::TeamCounts) {
        spawn_player();
    }

    int32 actor_index{0};
    for (int32 team_index{0}; team_index < expected_team_counts.Num(); ++team_index) {
        auto const team{static_cast<ETestTeam>(team_index)};
        for (int32 i{0}; i < expected_team_counts[team_index]; ++i) {
            auto* const capital{
                spawn_capital_proxy(context_.world,
                                    context_.config,
                                    checks,
                                    FName{FString::Printf(TEXT("capital_%d"), actor_index)},
                                    FVector{static_cast<float>(actor_index * 5000),
                                            static_cast<float>(team_index * 5000),
                                            4360.f})};
            if (!checks.is_valid(capital, TEXT("Registry capital is spawned"))) {
                return;
            }
            capital->set_team(team);
            capital->set_actor_config(capital_config);
            capital->set_initial_spawn_delay(5.f);
            capital->set_spawn_cooldown(60.f);
            ++actor_index;
        }
    }
}

void FEntityRegistryScenario::spawn_player() {
    auto const* const simulation_config{context_.config.simulation_config.Get()};
    if (!checks.not_nullptr(simulation_config, TEXT("Simulation config is available"))) {
        return;
    }
    auto* const player{spawn_player_ship(context_.world,
                                         context_.config.actor_classes.player_ship_class,
                                         simulation_config->player_ship_config)};
    if (!checks.is_valid(player, TEXT("Player ship is spawned"))) {
        return;
    }
    context_.orchestrator.set_player_ship(*player);
}

/* ------------------------------------------------------------------------------------------ */
// Team counts
/* ------------------------------------------------------------------------------------------ */
void FEntityRegistryScenario::begin_team_count_scenario() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    reset_and_reserve_time_series(
        test_driver->orchestrator, team_count_test_time, alive_per_team, alive_per_team_and_type);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FEntityRegistryScenario::on_team_count_end_tick));
    test_driver->timeline.finish_at(team_count_test_time);
}

void FEntityRegistryScenario::sample_team_counts() {
    auto const time{test_driver->get_time()};
    alive_per_team.add(time, test_driver->registry.count_alive_per_team());
    alive_per_team_and_type.add(time, test_driver->registry.count_alive_per_team_and_type());
}

void FEntityRegistryScenario::on_team_count_end_tick(ATestBatchOrchestrator&) {
    sample_team_counts();
    test_driver->advance_timeline();
}

void FEntityRegistryScenario::check_team_counts() {
    auto const sample_index{alive_per_team.nearest_index(team_count_test_time)};
    auto const& counts{alive_per_team.value_at(sample_index)};
    checks.are_equal(15, sum(TConstArrayView<int32>{counts}), TEXT("Check entity total"));
    for (int32 team_index{0}; team_index < expected_team_counts.Num(); ++team_index) {
        auto const team{static_cast<ETestTeam>(team_index)};
        checks.are_equal(
            expected_team_counts[team_index],
            counts[team_index],
            FString::Printf(TEXT("Count team %s"), *to_string_without_type_prefix(team)));

        auto const& type_counts{alive_per_team_and_type.value_at(sample_index)};
        int32 type_count{0};
        constexpr auto n_types{EnumCountTrait<ETestEntityType>::count_value};
        for (int32 type{0}; type < n_types; ++type) {
            type_count += type_counts[team_index][type];
        }
        checks.are_equal(counts[team_index],
                         type_count,
                         FString::Printf(TEXT("Count team/type matrix for %s"),
                                         *to_string_without_type_prefix(team)));
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

/* ------------------------------------------------------------------------------------------ */
// Player kills
/* ------------------------------------------------------------------------------------------ */
void FEntityRegistryScenario::begin_variable_kill_scenario() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();
    auto const& player{test_driver->get_player_ship()};
    player_id = player.get_unique_id();
    initial_alive_count = test_driver->registry.count_alive();
    auto const available_targets{test_driver->registry.get_handles_not_in_team(player.get_team())};
    if (!checks.is_greater_than(available_targets.Num(),
                                expected_kills - 1,
                                TEXT("Enough non-player-team targets are available"))) {
        return;
    }

    TArray<FRegistryEntityHandle> targets;
    targets.Append(available_targets.GetData(), expected_kills);
    reset_and_reserve_time_series(
        test_driver->orchestrator, variable_kill_test_duration, kill_samples);
    test_driver->orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FEntityRegistryScenario::on_variable_kill_end_tick));
    test_driver->timeline.then_after(kill_time,
                                     [this, targets, player_handle = player.get_entity_handle()] {
                                         test_driver->queue_kills(targets, player_handle);
                                     });
    test_driver->timeline.finish_at(variable_kill_test_duration);
}

void FEntityRegistryScenario::on_variable_kill_end_tick(ATestBatchOrchestrator&) {
    auto const& registry{test_driver->registry};
    kill_samples.add(test_driver->get_time(),
                     FVariableKillSample{
                         static_cast<int32>(registry.get_kills(player_id)),
                         registry.count_kills(),
                         registry.count_alive(),
                     });
    test_driver->advance_timeline();
}

void FEntityRegistryScenario::check_variable_kill_results() {
    checks.is_greater_than(kill_samples.num(), int32{0}, TEXT("Kill samples recorded"));
    if (kill_samples.is_empty()) {
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
        return;
    }
    auto const& before{kill_samples.nearest_value(before_kill_time)};
    auto const& after{kill_samples.nearest_value(after_kill_time)};
    auto const& final{kill_samples.nearest_value(variable_kill_test_duration)};
    checks.are_equal(0, before.player_kills, TEXT("Player kills are zero before event"));
    checks.are_equal(0, before.total_kills, TEXT("Total kills are zero before event"));
    checks.are_equal(
        initial_alive_count, before.alive_count, TEXT("All entities are alive before event"));
    checks.are_equal(
        expected_kills, after.player_kills, TEXT("Kills are attributed to player ship"));
    checks.are_equal(
        expected_kills, after.total_kills, TEXT("Total kill count matches killed entities"));
    checks.are_equal(initial_alive_count - expected_kills,
                     after.alive_count,
                     TEXT("Alive count reflects killed entities"));
    checks.are_equal(expected_kills, final.player_kills, TEXT("Player kill count remains correct"));
    checks.are_equal(expected_kills, final.total_kills, TEXT("Total kill count remains correct"));
    checks.are_equal(initial_alive_count - expected_kills,
                     final.alive_count,
                     TEXT("Alive count remains correct"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FEntityRegistryScenario::run() {
    if (scenario_ == EEntityRegistryScenario::TeamCounts) {
        run_until_timeline_finished([this] { begin_team_count_scenario(); },
                                    FTimespan{0, 0, 1},
                                    [this] { check_team_counts(); });
        return;
    }
    run_until_timeline_finished([this] { begin_variable_kill_scenario(); },
                                FTimespan{0, 0, 1},
                                [this] { check_variable_kill_results(); });
}
}

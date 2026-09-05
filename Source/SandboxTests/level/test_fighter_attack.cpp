#include "test_fighter_attack_scenario.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityRegistryData.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

namespace ml {
void run_worldless_fighter_attack(FAutomationTestBase& test,
                                  FSoftTestAssertions& checks,
                                  USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 25000.f;
    add_worldless_capital_spawn(
        data, FVector3f{-22020.f, 2170.f, 4360.f}, ETestTeam::Green, 1, 0.f, 60.f);
    add_worldless_capital_spawn(
        data, FVector3f{17030.f, 2170.f, 4360.f}, ETestTeam::Red, 0, 60.f, 60.f);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const* capitals{harness.get_simulation().get_capital_ships()};
    auto const* fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals->get_handle(1)};
    struct Sample {
        int32 enemy_health{};
        TArray<ETestTeam> fighter_teams;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.enemy_health = capitals->get_health(enemy)};
        sample.fighter_teams.Append(fighters->get_teams());
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.finish_at(11.0);
    test.TestTrue(TEXT("Fighter-attack timeline completes"),
                  harness.run_until_timeline_finished(12.0));
    checks.is_true(!samples.is_empty(), TEXT("Fighter-attack samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(1.0)};
    auto const& after{samples.nearest_value(11.0)};
    checks.is_greater_than(before.fighter_teams.Num(), int32{0}, TEXT("Hero fighters spawned"));
    for (int32 i{}; i < before.fighter_teams.Num(); ++i) {
        checks.are_equal(
            ETestTeam::Green, before.fighter_teams[i], TEXT("Fighter is on the hero team"), i);
    }
    checks.is_true(after.enemy_health < before.enemy_health, TEXT("Enemy lost health"));
}

FFighterAttackScenario::FFighterAttackScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FFighterAttackScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

/* ------------------------------------------------------------------------------------------ */
// Setup
/* ------------------------------------------------------------------------------------------ */
void FFighterAttackScenario::spawn_fixture() {
    auto* const level_config{duplicate_level_config(context_.config, context_.orchestrator)};
    if (!checks.not_nullptr(level_config, TEXT("Level config is duplicated"))) {
        return;
    }

    auto* const fighter_config{&level_config->fighters};
    if (!checks.not_nullptr(fighter_config, TEXT("Fighter attack config is created"))) {
        return;
    }
    fighter_config->laser.projectile_speed = 20000.f;
    fighter_config->laser.max_distance = 25000.f;
    fighter_config->visual_logger_style = nullptr;
    context_.orchestrator.set_level_config(*level_config);

    auto* const hero_proxy{spawn_capital_proxy(context_.world,
                                               context_.config,
                                               checks,
                                               TEXT("hero_capital"),
                                               FVector{-22020.f, 2170.f, 4360.f})};
    auto* const enemy_proxy{spawn_capital_proxy(context_.world,
                                                context_.config,
                                                checks,
                                                TEXT("enemy_capital"),
                                                FVector{17030.f, 2170.f, 4360.f})};
    if (!checks.is_valid(hero_proxy, TEXT("Hero capital is spawned")) ||
        !checks.is_valid(enemy_proxy, TEXT("Enemy capital is spawned"))) {
        return;
    }

    hero_proxy->set_team(hero_team);
    hero_proxy->set_target_ship(enemy_proxy);
    hero_proxy->set_spawn_cooldown(60.f);
    enemy_proxy->set_team(enemy_team);
    enemy_proxy->set_target_ship(hero_proxy);
    enemy_proxy->set_initial_spawn_delay(60.f);

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FFighterAttackScenario::bind_proxy_entities);
}

void FFighterAttackScenario::bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
    resolve_proxy_entity_bindings(
        proxy_entities,
        {{TEXT("hero_capital"), &hero, nullptr}, {TEXT("enemy_capital"), &enemy, nullptr}},
        checks);
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FFighterAttackScenario::initial_setup_and_stimuli() {
    initialise_test_driver();
    test_driver->orchestrator.start_simulation();

    checks.is_true(test_driver->get_registry().is_valid_handle(hero), TEXT("Read hero handle"));
    checks.is_true(test_driver->get_registry().is_valid_handle(enemy), TEXT("Read enemy handle"));
    checks.is_true(hero != enemy, TEXT("Hero and enemy handles are distinct"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    reset_and_reserve_time_series(
        test_driver->orchestrator, initial_wait + fight_duration, samples);
    test_driver->orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FFighterAttackScenario::on_end_tick));
    test_driver->timeline.at(initial_wait, [this] { t_pre_fight = test_driver->get_time(); })
        .then_after(fight_duration, [this] { t_post_fight = test_driver->get_time(); });
}

/* ------------------------------------------------------------------------------------------ */
// Samples and checks
/* ------------------------------------------------------------------------------------------ */
void FFighterAttackScenario::sample_values() {
    auto const& entity_data{test_driver->get_registry().get_entity_data()};
    FSimulationSample sample{};
    sample.enemy_health = test_driver->get_capital_ships().get_health(enemy);
    sample.fighter_teams.Append(test_driver->get_capital_ship_fighters().get_teams());
    sample.radii.Append(entity_data.radii);
    samples.add(test_driver->get_time(), MoveTemp(sample));
}

void FFighterAttackScenario::on_end_tick(ATestBatchOrchestrator&) {
    sample_values();
    test_driver->advance_timeline();
}

void FFighterAttackScenario::check_fighters_team(FSimulationSample const& sample) {
    check_all_teams_are(
        sample.fighter_teams, hero_team, checks, TEXT("Fighters are on hero team."));
}

void FFighterAttackScenario::full_checks() {
    auto const& pre_fight{samples.nearest_value(t_pre_fight)};
    auto const& post_fight{samples.nearest_value(t_post_fight)};
    check_fighters_team(pre_fight);
    check_radii(TConstArrayView<float>{pre_fight.radii}, checks, 0.05f);
    check_health_decreased(
        pre_fight.enemy_health, post_fight.enemy_health, checks, TEXT("Enemy lost health"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FFighterAttackScenario::run() {
    run_until_timeline_finished(
        [this] { initial_setup_and_stimuli(); }, timeout, [this] { full_checks(); });
}
}

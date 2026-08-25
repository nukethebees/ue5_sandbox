#include "test_fighter_attack_scenario.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipFightersConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

namespace ml {
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
    auto* const fighter_actor{
        const_cast<ATestCapitalShipFighters*>(context_.orchestrator.get_capital_ship_fighters())};
    if (!checks.is_valid(fighter_actor, TEXT("Fighter batch actor is available"))) {
        return;
    }

    auto* const fighter_config{
        duplicate_capital_ship_fighters_config(context_.config, *fighter_actor)};
    if (!checks.not_nullptr(fighter_config, TEXT("Fighter attack config is created"))) {
        return;
    }
    fighter_config->laser_speed = 20000.f;
    fighter_config->laser_max_distance = 25000.f;
    fighter_config->visual_logger_style = nullptr;
    fighter_actor->set_actor_config(fighter_config);

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

    checks.is_true(test_driver->registry.is_valid_handle(hero), TEXT("Read hero handle"));
    checks.is_true(test_driver->registry.is_valid_handle(enemy), TEXT("Read enemy handle"));
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
    auto const& entity_data{test_driver->registry.get_entity_data()};
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
    TestCommandBuilder.Do([this] { initial_setup_and_stimuli(); })
        .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
        .Then([this] { full_checks(); });
}
}

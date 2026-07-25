#include "lex_to_string.h"
#include "TestFighterAttackDriver.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <Sandbox/utilities/world.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

namespace {
struct FPreFight {
    int32 enemy_health;
};

struct FPostFight {
    int32 enemy_health;
};
}

TEST_CLASS(FighterCapitalAttack, "Sandbox.FunctionalTests")
{
    using time_type = ml::TestSimulationDriver::time_type;

    inline static FTimespan const default_timeout{0, 0, 2};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ATestFighterAttackDriver* local_driver{nullptr};

    FRegistryEntityHandle hero;
    ETestTeam hero_team;

    FRegistryEntityHandle enemy;
    ETestTeam enemy_team;

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_fighter_attack"), TestRunner, checks); }
  private:
    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{1.0};

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        local_driver = ml::get_first_actor<ATestFighterAttackDriver>(world);
    }
    void initial_checks() {
        checks.is_valid(local_driver, TEXT("Local driver is valid."));

        auto const setup_error{local_driver->get_setup_error()};
        checks.is_true(setup_error.IsEmpty(), TEXT("Checking local driver setup"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void initial_samples() {
        auto const& capitals{test_driver->get_capital_ships()};

        hero_team = local_driver->get_hero_team();
        enemy_team = local_driver->get_enemy_team();

        auto const maybe_hero{capitals.find_first_handle_on_team(hero_team)};
        auto const maybe_enemy{capitals.find_first_handle_on_team(enemy_team)};

        checks.is_true(maybe_hero.has_value(), TEXT("Read hero handle"));
        checks.is_true(maybe_enemy.has_value(), TEXT("Read enemy handle"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        hero = *maybe_hero;
        enemy = *maybe_enemy;
    }
    void initial_phase() {
        initial_setup();
        initial_samples();
        initial_checks();

        test_driver->set_delta_time_wait(initial_wait);
    }

    /* ---------------------------------------------------------------------------- */
    // Fight phase
    /* ---------------------------------------------------------------------------- */
    FPreFight pre_fight;
    FPostFight post_fight;

    void pre_fight_samples() {
        auto const& capitals{test_driver->get_capital_ships()};

        pre_fight.enemy_health = capitals.get_health(enemy);
    }
    void check_fighters_team() {
        auto const& fighters{test_driver->get_capital_ship_fighters()};
        auto const teams{fighters.get_teams()};

        auto const n{teams.Num()};
        for (int32 i{0}; i < n; ++i) {
            checks.are_equal(hero_team, teams[i], TEXT("Fighters are on hero team."));
        }
    }
    void pre_fight_checks() {
        check_fighters_team();
    }
    void pre_fight_phase() {
        pre_fight_samples();
        pre_fight_checks();

        test_driver->set_delta_time_wait(local_driver->get_fight_duration());
    }

    // Post fight
    void post_fight_samples() {
        auto const& capitals{test_driver->get_capital_ships()};

        post_fight.enemy_health = capitals.get_health(enemy);
    }
    void post_fight_checks() {
        checks.is_true(post_fight.enemy_health < pre_fight.enemy_health, TEXT("Enemy lost health"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void post_fight_phase() {
        post_fight_samples();
        post_fight_checks();
    }

    TEST_METHOD(Main)
    {
        auto check_wait_over{[this] { return test_driver->time_wait_completed(); }};

        TestCommandBuilder
            // Initial phase
            .Do([this] { initial_phase(); })
            .Until(check_wait_over)
            // Fight
            .Then([this] { pre_fight_phase(); })
            .Until(check_wait_over)
            .Then([this] { post_fight_phase(); });
    }
};

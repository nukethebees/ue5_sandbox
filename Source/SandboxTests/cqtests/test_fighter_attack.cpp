#include "lex_to_string.h"
#include "TestFighterAttackDriver.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/utilities/world.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/test_timeline.h>
#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(FighterCapitalAttack, "Sandbox.FunctionalTests")
{
    using time_type = ml::TestSimulationDriver::time_type;
    using ThisClass = FighterCapitalAttack;

    struct FSimulationSample {
        int32 enemy_health{0};
        TArray<ETestTeam> fighter_teams;
        TArray<float> radii;
    };

    inline static FTimespan const default_timeout{0, 0, 12};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ATestFighterAttackDriver* local_driver{nullptr};

    FRegistryEntityHandle hero;
    ETestTeam hero_team;

    FRegistryEntityHandle enemy;
    ETestTeam enemy_team;

    ml::TimeSeriesData<FSimulationSample> samples;
    FTestTimeline timeline;
    time_type t_pre_fight{0.0};
    time_type t_post_fight{0.0};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_fighter_attack"), TestRunner, checks); }
    AFTER_EACH()
    { test_driver->orchestrator.clear_end_tick_test_hook(); }
  private:
    /* ---------------------------------------------------------------------------- */
    // Initial phase
    /* ---------------------------------------------------------------------------- */
    static constexpr time_type initial_wait{1.0};

    void sample_values(ATestBatchOrchestrator&) {
        auto const t{test_driver->get_time()};
        auto const& entity_data{test_driver->registry.get_entity_data()};

        FSimulationSample sample{};
        sample.enemy_health = test_driver->get_capital_ships().get_health(enemy);
        sample.fighter_teams.Append(test_driver->get_capital_ship_fighters().get_teams());
        sample.radii.Append(entity_data.radii);
        samples.add(t, MoveTemp(sample));
    }
    void on_end_tick(ATestBatchOrchestrator & orchestrator) {
        sample_values(orchestrator);
        timeline.tick(test_driver->get_time());
    }

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        test_driver->orchestrator.start_simulation();
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
    void initial_setup_and_stimuli() {
        initial_setup();
        initial_samples();
        initial_checks();
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        timeline.at(initial_wait, [this] { t_pre_fight = test_driver->get_time(); })
            .finish_after(local_driver->get_fight_duration(),
                          [this] { t_post_fight = test_driver->get_time(); });
    }

    /* ---------------------------------------------------------------------------- */
    // Fight phase
    /* ---------------------------------------------------------------------------- */
    void check_fighters_team(FSimulationSample const& sample) {
        auto const& teams{sample.fighter_teams};

        auto const n{teams.Num()};
        for (int32 i{0}; i < n; ++i) {
            checks.are_equal(hero_team, teams[i], TEXT("Fighters are on hero team."));
        }
    }
    void full_checks() {
        auto const& pre_fight{samples.nearest_value(t_pre_fight)};
        auto const& post_fight{samples.nearest_value(t_post_fight)};

        check_fighters_team(pre_fight);
        for (int32 i{0}; i < pre_fight.radii.Num(); ++i) {
            checks.is_greater_than(pre_fight.radii[i], 0.05f, TEXT("Check radii > 0.05"), i);
        }

        checks.is_true(post_fight.enemy_health < pre_fight.enemy_health, TEXT("Enemy lost health"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    TEST_METHOD(Main)
    {
        TestCommandBuilder.Do([this] { initial_setup_and_stimuli(); })
            .Until([this] { return timeline.is_finished(); }, default_timeout)
            .Then([this] { full_checks(); });
    }
};

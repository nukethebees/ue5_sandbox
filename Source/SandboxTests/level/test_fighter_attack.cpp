#include <SandboxTests/support/lex_to_string.h>
#include <SandboxTests/support/TestFighterAttackDriver.h>

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SandboxCore/time_series_data.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Misc/Optional.h>

TEST_CLASS(FighterCapitalAttack, "Sandbox.LevelTests")
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
    time_type t_pre_fight{0.0};
    time_type t_post_fight{0.0};

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_fighter_attack"), TestRunner, checks); }
    AFTER_EACH()
    {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
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
        test_driver->timeline.tick(test_driver->get_time());
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
        hero_team = local_driver->get_hero_team();
        enemy_team = local_driver->get_enemy_team();

        hero = local_driver->get_hero_handle();
        enemy = local_driver->get_enemy_handle();

        checks.is_true(test_driver->registry.is_valid_handle(hero), TEXT("Read hero handle"));
        checks.is_true(test_driver->registry.is_valid_handle(enemy), TEXT("Read enemy handle"));
        checks.is_true(hero != enemy, TEXT("Hero and enemy handles are distinct"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    void initial_setup_and_stimuli() {
        initial_setup();
        initial_samples();
        initial_checks();

        auto const test_duration{initial_wait + local_driver->get_fight_duration()};
        ml::reset_and_reserve_time_series(test_driver->orchestrator, test_duration, samples);

        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.at(initial_wait, [this] { t_pre_fight = test_driver->get_time(); })
            .then_after(local_driver->get_fight_duration(),
                        [this] { t_post_fight = test_driver->get_time(); });
    }

    /* ---------------------------------------------------------------------------- */
    // Fight phase
    /* ---------------------------------------------------------------------------- */
    void check_fighters_team(FSimulationSample const& sample) {
        ml::check_all_teams_are(
            sample.fighter_teams, hero_team, checks, TEXT("Fighters are on hero team."));
    }
    void full_checks() {
        auto const& pre_fight{samples.nearest_value(t_pre_fight)};
        auto const& post_fight{samples.nearest_value(t_post_fight)};

        check_fighters_team(pre_fight);
        ml::check_radii(TConstArrayView<float>{pre_fight.radii}, checks, 0.05f);

        ml::check_health_decreased(
            pre_fight.enemy_health, post_fight.enemy_health, checks, TEXT("Enemy lost health"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }
    TEST_METHOD(Main)
    {
        TestCommandBuilder.Do([this] { initial_setup_and_stimuli(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, default_timeout)
            .Then([this] { full_checks(); });
    }
};

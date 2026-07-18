#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>
#include <Sandbox/utilities/enums.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/array_math.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Map.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(TestEntityRegistry, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::FSoftTestAssertions<std::remove_cvref_t<decltype(*TestRunner)>> checks{};

    TMap<ETestTeam, int32> expected_teams{
        {ETestTeam::White, 0},
        {ETestTeam::Red, 1},
        {ETestTeam::Green, 2},
        {ETestTeam::Blue, 3},
        {ETestTeam::Orange, 4},
        {ETestTeam::Yellow, 5},
    };

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_entity_registry"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);

        checks.test_runner = TestRunner;
        checks.all_passed = true;
#if WITH_EDITOR
        auto const* settings{GetDefault<USandboxDeveloperSettings>()};
        checks.log_successful_assertions = settings->log_successful_assertions;
#endif
    }
  private:
    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
    }

    auto get_total_expected() const -> int32 {
        int32 total{0};

        for (auto const& [_, v] : expected_teams) {
            total += v;
        }

        return total;
    }

    void run_checks() {
        count_teams();

        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    auto get_expected_team_counts_msg() const -> FString {
        FString msg{TEXT("Expected team counts:")};

        for (auto const& [k, v] : expected_teams) {
            msg += FString::Printf(TEXT("\n    %s: %d"), *ml::to_string_without_type_prefix(k), v);
        }

        return msg;
    }
    auto get_team_counts(TConstArrayView<int32> const counts) const -> FString {
        FString msg;

        auto const n{ml::EnumCountTrait<ETestEntityType>::count_value};
        for (int32 i{}; i < n; ++i) {
            msg +=
                FString::Printf(TEXT("\n%s: %d"),
                                *ml::to_string_without_type_prefix(static_cast<ETestEntityType>(i)),
                                counts[i]);
        }

        return msg;
    }

    void count_teams() {
        TMap<ETestTeam, int32> teams_counted;

        auto const counts{test_driver->registry.count_alive_per_team()};
        auto const counts_view{TConstArrayView<int32>(counts)};
        auto const count_sum{ml::sum(counts_view)};

        checks.are_equal(get_total_expected(), count_sum, [&] -> FString {
            FString msg{TEXT("Check entity total.")};
            msg += get_team_counts(counts_view);
            return msg;
        }());

        auto const n_teams{ml::EnumCountTrait<ETestTeam>::count_value};
        for (int32 i{}; i < n_teams; ++i) {
            teams_counted.Emplace(static_cast<ETestTeam>(i), counts[i]);
        }

        for (auto const& [team, exp_count] : expected_teams) {
            auto const count{counts[std::to_underlying(team)]};
            checks.are_equal(exp_count, count, [&] -> FString {
                return FString::Printf(TEXT("Count team %s"),
                                       *ml::to_string_without_type_prefix(team));
            }());
        }
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.Do([this] {
            initial_setup();
            run_checks();
        });
    }
};

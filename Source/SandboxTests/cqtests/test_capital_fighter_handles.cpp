#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>

#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(CapitalFighterHandles, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};

    ATestBatchOrchestrator const* orchestrator{nullptr};
    ATestEntityRegistry const* registry{nullptr};
    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    uint64 tick_threshold{0};

    bool all_passed{true};
    bool log_successful_assertions{false};

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_capital_fighter_handles"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);

        all_passed = true;

#if WITH_EDITOR
        auto const* settings{GetDefault<USandboxDeveloperSettings>()};
        log_successful_assertions = settings->log_successful_assertions;
#endif
    }
  protected:
    inline static auto to_string(bool b) -> TCHAR const* {
        return b ? TEXT("true") : TEXT("false");
    }

    void display_result(bool const passed, FString const& msg) {
        if (!passed) {
            TestRunner->AddError(msg);
        } else if (log_successful_assertions) {
            TestRunner->AddInfo(msg);
        }
    }

    void store_result(bool const result) noexcept {
        all_passed &= result;
    }

    template <typename T>
    bool are_equal(T const& exp, T const& got, FString const description) {
        auto const result{exp == got};
        store_result(result);

        auto const exp_str{LexToString(exp)};
        auto const got_str{LexToString(got)};
        auto msg{FString::Printf(TEXT("%s (Exp %s, Got %s)"), *description, *exp_str, *got_str)};

        display_result(result, msg);

        return result;
    }
    bool is_true(bool result, FString const description) {
        store_result(result);

        auto msg{FString::Printf(TEXT("%s (%s)"), *description, to_string(result))};

        display_result(result, msg);

        return result;
    }

    void initial_setup() {
        auto& world{spawner->GetWorld()};

        for (TActorIterator<ATestBatchOrchestrator> it(&world); it; ++it) {
            orchestrator = *it;
            break;
        }
        ASSERT_THAT(IsNotNull(orchestrator));

        registry = orchestrator->get_entity_registry();
        ASSERT_THAT(IsNotNull(registry));

        capitals = orchestrator->get_capital_ships();
        ASSERT_THAT(IsNotNull(capitals));

        fighters = orchestrator->get_capital_ship_fighters();
        ASSERT_THAT(IsNotNull(fighters));

        test_driver = ml::TestSimulationDriver{*orchestrator->get_entity_registry(), world};
    }
    bool wait_for_fighters_to_spawn() {
        auto const n_fighters{capitals->get_fighters_spawned()};
        if (n_fighters > 0) { return true; }

        return false;
    }
    void run_spawn_capital_handle_checks() {
        auto const n_capitals_exp{2};
        auto const n_capitals{capitals->get_num_instances()};

        are_equal(n_capitals_exp, n_capitals, TEXT("Number of capitals."));

        auto const capital_fighters_spawn_slots{capitals->get_fighter_spawn_slots()};

        auto const n_fighters_spawned_exp{n_capitals * capital_fighters_spawn_slots};
        auto const n_fighters_spawned{capitals->get_fighters_spawned()};

        are_equal(n_fighters_spawned_exp,
                  n_fighters_spawned,
                  TEXT("Number of fighters (according to capitals)"));

        auto const n_fighters_exp{n_fighters_spawned};
        auto const n_fighters{fighters->get_num_instances()};

        are_equal(n_fighters_exp, n_fighters, TEXT("Number of fighters."));

        auto const& capital_fighter_handles{capitals->get_fighter_handles()};
        auto const& capital_fighter_handle_spans{capitals->get_capital_fighter_handle_spans()};

        auto const n_fighter_handles_exp{n_capitals * capital_fighters_spawn_slots};
        auto const n_fighter_handles{capital_fighter_handles.Num()};
        are_equal(n_fighter_handles_exp, n_fighter_handles, TEXT("Number of fighter handles"));

        auto const n_fighter_spans_exp{n_capitals};
        auto const n_fighter_spans{capital_fighter_handle_spans.Num()};
        are_equal(n_fighter_spans_exp, n_fighter_spans, TEXT("Fighter handle spans"));

        for (int32 i{0}; i < n_capitals; ++i) {
            auto const span{capital_fighter_handle_spans[i]};

            auto const span_count_exp{capital_fighters_spawn_slots};
            auto const span_count{span.count};

            are_equal(span_count_exp, span_count, FString::Printf(TEXT("Span count (%d)"), i));
        }

        // Check handle uniqueness
        TSet<FRegistryEntityHandle> unique_handles;
        for (int32 i{}; i < n_fighter_handles; ++i) {
            auto const& handle{capital_fighter_handles[i]};

            auto const is_unique{
                is_true(!unique_handles.Contains(handle),
                        FString::Printf(TEXT("Unique capital fighter handle (%d)"), i))};
            if (is_unique) { unique_handles.Add(handle); }
        }

        if (!all_passed) {
            FString msg;
            msg += FString::Printf(TEXT("Failed on tick: %llu"), orchestrator->get_tick_count());

            msg += TEXT("\nCapital ship fighter handles:");
            for (auto const& handle : capital_fighter_handles) {
                msg += FString::Printf(TEXT("\n    %s"), *handle.to_string());
            }

            msg += TEXT("\nCapital ship fighter handles spans:");
            for (int32 span_i{0}; span_i < n_capitals; ++span_i) {
                msg += FString::Printf(TEXT("\n        Capital %d"), span_i);

                auto const& span{capital_fighter_handle_spans[span_i]};

                auto const offset{span.offset};
                auto const count{span.count};
                auto const end{span.end()};

                for (int32 i{offset}; i < end; ++i) {
                    msg += FString::Printf(
                        TEXT("\n    %d: %s"), i, *capital_fighter_handles[i].to_string());
                }
            }

            TestRunner->AddInfo(msg);
        }

        ASSERT_THAT(IsTrue(all_passed, TEXT("all_passed")));
    }

    void kill_fighters() {
        auto const fighter_handles{capitals->get_fighter_handles()};

        for (auto const handle : fighter_handles) {
            test_driver->queue_damage(handle, 100000, {});
        }

        tick_threshold = orchestrator->get_tick_count() + 5;
    }
    void check_handles_after_kills() {}

    TEST_METHOD(MainTest)
    {

        FTimespan const timeout{0, 0, 3};
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this]() -> bool { return wait_for_fighters_to_spawn(); }, timeout)
            .Then([this] { run_spawn_capital_handle_checks(); })
            .Then([this] { kill_fighters(); })
            .Until([this]() -> bool { return orchestrator->get_tick_count() >= tick_threshold; },
                   timeout)
            .Then([this] { check_handles_after_kills(); });
    }
};

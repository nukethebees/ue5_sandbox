#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestLasers.h>

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <Engine/HitResult.h>
#include <Misc/Optional.h>

TEST_CLASS(SpatialQueryManager, "Sandbox.LevelTests")
{
    using ThisClass = SpatialQueryManager;
    using time_type = ml::TestSimulationDriver::time_type;

    struct FResolutionSample {
        FRegistryEntityHandle single_result{};
        TArray<FRegistryEntityHandle> mixed_results;
        TArray<FRegistryEntityHandle> expected_results;
    };

    static constexpr time_type test_time{2.5};
    FTimespan const timeout{0, 0, 4};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks{};
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};
    ml::TimeSeriesData<FResolutionSample> samples;

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_capital_fighter_handles"), TestRunner, checks); }
    AFTER_EACH()
    {
        if (test_driver.IsSet()) {
            test_driver->orchestrator.clear_end_tick_test_hook();
            test_driver->orchestrator.pause_simulation();
        }
    }
  private:
    static auto make_hit(AActor & actor, UPrimitiveComponent & component, int32 const item)
        -> FHitResult {
        FHitResult hit{&actor, &component, FVector::ZeroVector, FVector::UpVector};
        hit.Item = item;
        return hit;
    }

    void initial_setup() {
        test_driver = ml::TestSimulationDriver::from_world(spawner->GetWorld());
        test_driver->orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::on_end_tick));
        test_driver->timeline.finish_at(test_time);
        test_driver->orchestrator.start_simulation();
    }

    void sample_resolution() {
        auto& orchestrator{test_driver->orchestrator};
        auto* const capitals{const_cast<ATestCapitalShips*>(orchestrator.get_capital_ships())};
        auto* const fighters{
            const_cast<ATestCapitalShipFighters*>(orchestrator.get_capital_ship_fighters())};
        auto* const lasers{const_cast<ATestLasers*>(orchestrator.get_lasers())};

        if (!capitals || !fighters || !lasers || (capitals->get_num_instances() < 2) ||
            (fighters->get_num_instances() < 2)) {
            return;
        }

        auto* const capital_instances{
            capitals->FindComponentByClass<UInstancedStaticMeshComponent>()};
        auto* const fighter_instances{
            fighters->FindComponentByClass<UInstancedStaticMeshComponent>()};
        auto* const laser_instances{lasers->FindComponentByClass<UInstancedStaticMeshComponent>()};
        if (!capital_instances || !fighter_instances || !laser_instances) {
            return;
        }

        TArray<FHitResult> single_hit{make_hit(*capitals, *capital_instances, 0)};
        TArray<FRegistryEntityHandle> single_result{FRegistryEntityHandle{}};
        orchestrator.get_spatial_query_manager().resolve_hits(single_hit, single_result);

        TArray<FHitResult> mixed_hits{
            make_hit(*capitals, *capital_instances, 0),
            make_hit(*lasers, *laser_instances, 0),
            make_hit(*fighters, *fighter_instances, 1),
            make_hit(*capitals, *capital_instances, 1),
            make_hit(*capitals, *laser_instances, 0),
            make_hit(*fighters, *fighter_instances, 0),
        };
        TArray<FRegistryEntityHandle> mixed_results;
        mixed_results.Init(capitals->get_handle(0), mixed_hits.Num());
        orchestrator.get_spatial_query_manager().resolve_hits(mixed_hits, mixed_results);

        FResolutionSample sample;
        sample.single_result = single_result[0];
        sample.mixed_results = MoveTemp(mixed_results);
        sample.expected_results = {
            capitals->get_handle(0),
            FRegistryEntityHandle{},
            fighters->get_handles()[1],
            capitals->get_handle(1),
            FRegistryEntityHandle{},
            fighters->get_handles()[0],
        };
        samples.add(test_driver->get_time(), MoveTemp(sample));
    }

    void on_end_tick(ATestBatchOrchestrator&) {
        sample_resolution();
        test_driver->timeline.tick(test_driver->get_time());
    }

    void run_checks() {
        checks.is_greater_than(
            samples.num(), int32{0}, TEXT("A hit-resolution sample was captured"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const sample_index{samples.nearest_index(test_time)};
        auto const& sample{samples.value_at(sample_index)};

        checks.are_equal(sample.expected_results[0],
                         sample.single_result,
                         TEXT("Single hit resolves to the capital entity"));
        checks.are_equal(sample.expected_results.Num(),
                         sample.mixed_results.Num(),
                         TEXT("One output is returned for every hit"));

        auto const n{sample.expected_results.Num()};
        for (int32 i{}; i < n; ++i) {
            checks.are_equal(sample.expected_results[i],
                             sample.mixed_results[i],
                             FString::Printf(TEXT("Resolved handle at hit index %d"), i));
        }
        checks.is_true(sample.mixed_results[1].is_null(), TEXT("Unknown actor resolves to null"));
        checks.is_true(sample.mixed_results[4].is_null(),
                       TEXT("Unknown component resolves to null"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    TEST_METHOD(ResolvesHitBatches)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] { return test_driver->timeline.is_finished(); }, timeout)
            .Then([this] { run_checks(); });
    }
};

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <SandboxCore/time_series_data.h>

#include <Commands/TestCommandBuilder.h>
#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <HAL/PlatformTime.h>
#include <Misc/CommandLine.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>

namespace ml::frame_memory_level_benchmark {
inline constexpr TCHAR map_directory[]{
    TEXT("Levels/FeatureTests/FT_soa_turrets/test_levels/benchmark")};
inline constexpr TCHAR map_name[]{TEXT("Batch_benchmark")};
inline constexpr double default_simulated_seconds{20.0};
inline constexpr double benchmark_time_scale{100.0};

struct FSample {
    SIZE_T claimed_bytes{};
    SIZE_T payload_bytes{};
    SIZE_T padding_bytes{};
    uint64 root_claim_count{};
    uint64 overflow_count{};
    int32 fighter_count{};
    int32 turrets_targeting_fighters{};
};
}

TEST_CLASS(FrameMemoryLevelBenchmark, "SandboxBenchmarks.FrameMemoryLevel")
{
    TUniquePtr<FMapTestSpawner> spawner_{nullptr};
    ml::FSoftTestAssertions checks_{};
    ml::TimeSeriesData<ml::frame_memory_level_benchmark::FSample> samples_{};

    BEFORE_EACH()
    {
        checks_.test_runner = TestRunner;
        checks_.all_passed = true;
        auto const directory{FPaths::Combine(FPaths::ProjectContentDir(),
                                             ml::frame_memory_level_benchmark::map_directory)};
        spawner_ =
            MakeUnique<FMapTestSpawner>(directory, ml::frame_memory_level_benchmark::map_name);
        spawner_->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    {
        spawner_.Reset();
        samples_.reset();
    }

    TEST_METHOD(TwentySimulationSeconds)
    {
        TestCommandBuilder.Do([this] {
            run_benchmark(ml::test_static_turrets::EScratchAllocationMode::Persistent,
                          TEXT("persistent_tarray"));
        });
    }

    TEST_METHOD(TwentySimulationSecondsDirectRoot)
    {
        TestCommandBuilder.Do([this] {
            run_benchmark(ml::test_static_turrets::EScratchAllocationMode::DirectRoot,
                          TEXT("direct_root"));
        });
    }

    TEST_METHOD(TwentySimulationSecondsLocalMonotonic)
    {
        TestCommandBuilder.Do([this] {
            run_benchmark(ml::test_static_turrets::EScratchAllocationMode::LocalMonotonic,
                          TEXT("local_monotonic"));
        });
    }
  private:
    void sample_tick(ATestBatchOrchestrator & orchestrator) {
        auto const* const simulation{orchestrator.get_level_simulation()};
        auto const* const turrets{orchestrator.get_turrets()};
        auto const* const fighters{orchestrator.get_capital_ship_fighters()};
        check(simulation);
        check(turrets);
        check(fighters);

        auto const& registry{orchestrator.get_entity_registry()};
        int32 turrets_targeting_fighters{};
        for (auto const target : turrets->get_target_handles()) {
            if (registry.is_valid_alive(target) &&
                registry.get_entity_type(target) == ETestEntityType::CapitalShipFighter) {
                ++turrets_targeting_fighters;
            }
        }

        auto const stats{simulation->get_frame_memory_stats()};
        samples_.add(orchestrator.get_simulation_time(),
                     ml::frame_memory_level_benchmark::FSample{
                         .claimed_bytes = stats.current_frame_peak_claimed_bytes,
                         .payload_bytes = stats.current_payload_bytes,
                         .padding_bytes = stats.current_padding_bytes,
                         .root_claim_count = stats.current_root_claim_count,
                         .overflow_count = stats.overflow_count,
                         .fighter_count = fighters->get_num_instances(),
                         .turrets_targeting_fighters = turrets_targeting_fighters,
                     });
    }

    void run_benchmark(ml::test_static_turrets::EScratchAllocationMode const allocation_mode,
                       TCHAR const* const variant_name) {
        auto& world{spawner_->GetWorld()};
        auto driver{ml::TestSimulationDriver::from_world(world)};
        auto& orchestrator{driver.orchestrator};
        auto* const simulation{orchestrator.get_level_simulation()};
        checks_.is_true(simulation != nullptr, TEXT("Benchmark level simulation is available"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks_);

        if (orchestrator.get_state() == EOrchestratorState::Running) {
            orchestrator.pause_simulation();
        }
        orchestrator.get_turrets()->set_scratch_allocation_mode(allocation_mode);
        driver.set_time_scale(ml::frame_memory_level_benchmark::benchmark_time_scale);
        orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &FrameMemoryLevelBenchmark::sample_tick));
        orchestrator.start_simulation();

        auto simulated_seconds{ml::frame_memory_level_benchmark::default_simulated_seconds};
        FParse::Value(FCommandLine::Get(),
                      TEXT("SandboxFrameMemoryLevelBenchmarkSeconds="),
                      simulated_seconds);
        simulated_seconds = FMath::Max(simulated_seconds, 0.1);

        auto const start_tick{orchestrator.get_completed_ticks()};
        auto const initial_capitals{driver.get_capital_ships().get_num_instances()};
        auto const initial_turrets{orchestrator.get_turrets()->get_num_instances()};
        auto const tick_period{simulation->get_clock().get_tick_period()};
        auto const ticks_to_run{
            static_cast<uint64>(FMath::RoundToInt64(simulated_seconds / tick_period))};
        samples_.reserve(static_cast<int32>(ticks_to_run));

        auto const started_at{FPlatformTime::Seconds()};
        constexpr uint64 ticks_per_advance{100};
        while (orchestrator.get_completed_ticks() - start_tick < ticks_to_run &&
               orchestrator.get_state() == EOrchestratorState::Running) {
            auto const completed_ticks{orchestrator.get_completed_ticks() - start_tick};
            auto const batch_ticks{FMath::Min(ticks_per_advance, ticks_to_run - completed_ticks)};
            auto const unscaled_dt{static_cast<double>(batch_ticks) * tick_period /
                                   ml::frame_memory_level_benchmark::benchmark_time_scale};
            orchestrator.tick(unscaled_dt);
        }
        auto const elapsed_seconds{FPlatformTime::Seconds() - started_at};
        orchestrator.pause_simulation();
        orchestrator.clear_end_tick_test_hook();

        checks_.is_true(!samples_.is_empty(), TEXT("Benchmark sampled fixed simulation ticks"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks_);

        SIZE_T peak_claimed_bytes{};
        SIZE_T peak_payload_bytes{};
        SIZE_T total_padding_bytes{};
        uint64 total_root_claims{};
        uint64 overflow_count{};
        int32 peak_fighters{};
        int32 peak_turrets_targeting_fighters{};
        for (auto const& sample : samples_.values()) {
            peak_claimed_bytes = FMath::Max(peak_claimed_bytes, sample.claimed_bytes);
            peak_payload_bytes = FMath::Max(peak_payload_bytes, sample.payload_bytes);
            total_padding_bytes += sample.padding_bytes;
            total_root_claims += sample.root_claim_count;
            overflow_count = FMath::Max(overflow_count, sample.overflow_count);
            peak_fighters = FMath::Max(peak_fighters, sample.fighter_count);
            peak_turrets_targeting_fighters =
                FMath::Max(peak_turrets_targeting_fighters, sample.turrets_targeting_fighters);
        }

        auto const completed_ticks{orchestrator.get_completed_ticks() - start_tick};
        auto const persistent_scratch_bytes{
            orchestrator.get_turrets()->get_persistent_scratch_allocated_bytes()};
        auto const ticks_per_second{static_cast<double>(completed_ticks) / elapsed_seconds};
        auto const mean_tick_microseconds{elapsed_seconds * 1'000'000.0 /
                                          static_cast<double>(completed_ticks)};
        TestRunner->AddInfo(FString::Printf(
            TEXT("Frame memory level benchmark: variant=%s, map=/Game/%s/%s.%s, "
                 "time_scale=%.1f, "
                 "initial_capitals=%d, final_capitals=%d, initial_turrets=%d, final_turrets=%d, "
                 "simulated_seconds=%.3f, ticks=%llu, "
                 "elapsed_seconds=%.6f, ticks_per_second=%.3f, "
                 "mean_tick_us=%.3f, persistent_scratch_bytes=%llu, "
                 "peak_claimed_bytes=%llu, peak_payload_bytes=%llu, "
                 "total_padding_bytes=%llu, total_root_claims=%llu, peak_fighters=%d, "
                 "peak_turrets_targeting_fighters=%d, lasers_spawned=%d"),
            variant_name,
            ml::frame_memory_level_benchmark::map_directory,
            ml::frame_memory_level_benchmark::map_name,
            ml::frame_memory_level_benchmark::map_name,
            ml::frame_memory_level_benchmark::benchmark_time_scale,
            initial_capitals,
            driver.get_capital_ships().get_num_instances(),
            initial_turrets,
            orchestrator.get_turrets()->get_num_instances(),
            static_cast<double>(completed_ticks) * tick_period,
            completed_ticks,
            elapsed_seconds,
            ticks_per_second,
            mean_tick_microseconds,
            static_cast<uint64>(persistent_scratch_bytes),
            static_cast<uint64>(peak_claimed_bytes),
            static_cast<uint64>(peak_payload_bytes),
            static_cast<uint64>(total_padding_bytes),
            total_root_claims,
            peak_fighters,
            peak_turrets_targeting_fighters,
            orchestrator.get_lasers()->get_number_spawned()));

        checks_.are_equal(static_cast<uint64>(ticks_to_run),
                          completed_ticks,
                          TEXT("Benchmark completes the requested simulation duration"));
        checks_.are_equal(uint64{0}, overflow_count, TEXT("Frame memory never overflows"));
        if (allocation_mode == ml::test_static_turrets::EScratchAllocationMode::Persistent) {
            checks_.is_true(persistent_scratch_bytes > 0,
                            TEXT("Persistent baseline provisions reusable scratch"));
        }
        checks_.is_true(peak_claimed_bytes > 0, TEXT("Simulation scratch uses frame memory"));
        checks_.is_true(peak_fighters > 0, TEXT("Capitals launch fighters"));
        checks_.is_true(peak_turrets_targeting_fighters > 0,
                        TEXT("Turrets engage fighters during the benchmark"));
        checks_.is_true(orchestrator.get_lasers()->get_number_spawned() > 0,
                        TEXT("The benchmark produces combat fire"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks_);
    }
};

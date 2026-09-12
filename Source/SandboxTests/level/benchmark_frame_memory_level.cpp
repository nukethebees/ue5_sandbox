#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <SandboxCore/time_series_data.h>

#include <Commands/TestCommandBuilder.h>
#include <CQTest.h>
#include <HAL/PlatformTime.h>
#include <Misc/CommandLine.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <UObject/Package.h>

namespace ml::frame_memory_level_benchmark {
inline constexpr TCHAR script_path[]{TEXT("LevelScripts/Benchmarks/Batch_benchmark.scm")};
inline constexpr double default_simulated_seconds{20.0};
inline constexpr double benchmark_time_scale{100.0};

struct FSample {
    SIZE_T claimed_bytes{};
    SIZE_T payload_bytes{};
    SIZE_T padding_bytes{};
    uint64 root_claim_count{};
    uint64 overflow_count{};
    int32 fighter_count{};
};
}

TEST_CLASS(FrameMemoryLevelBenchmark, "SandboxBenchmarks.FrameMemoryLevel")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup_{};

    ml::FSoftTestAssertions checks_{};
    ml::TimeSeriesData<ml::frame_memory_level_benchmark::FSample> samples_{};

    BEFORE_EACH()
    {
        checks_.test_runner = TestRunner;
        checks_.all_passed = true;
        level_setup_.begin_test(TestCommandBuilder, *TestRunner, checks_);
    }

    AFTER_EACH()
    {
        level_setup_.end_test();
        samples_.reset();
    }

    AFTER_ALL()
    { level_setup_.teardown(); }

    TEST_METHOD(TwentySimulationSeconds)
    {
        TestCommandBuilder.Do([this] { run_benchmark(); });
    }
  private:
    void sample_tick(ATestBatchOrchestrator & orchestrator) {
        auto const* const simulation{orchestrator.get_level_simulation()};
        auto const* const fighters{orchestrator.get_capital_ship_fighters()};
        check(simulation);
        check(fighters);

        auto const stats{simulation->get_frame_memory_stats()};
        samples_.add(orchestrator.get_simulation_time(),
                     ml::frame_memory_level_benchmark::FSample{
                         .claimed_bytes = stats.current_frame_peak_claimed_bytes,
                         .payload_bytes = stats.current_payload_bytes,
                         .padding_bytes = stats.current_padding_bytes,
                         .root_claim_count = stats.current_root_claim_count,
                         .overflow_count = stats.overflow_count,
                         .fighter_count = fighters->get_num_instances(),
                     });
    }

    void run_benchmark() {
        auto* const benchmark_orchestrator{level_setup_.get_orchestrator()};
        if (!checks_.is_valid(benchmark_orchestrator,
                              TEXT("Benchmark orchestrator is available"))) {
            return;
        }

        auto* const benchmark_config{
            ml::duplicate_level_config(level_setup_.get_config(), *GetTransientPackage())};
        if (!checks_.not_nullptr(benchmark_config, TEXT("Benchmark level config loads"))) {
            return;
        }
        benchmark_config->collision_grid.grid_size.Z = 500000.f;
        if (!checks_.is_true(benchmark_config->is_valid(),
                             TEXT("Benchmark level config is valid"))) {
            return;
        }
        benchmark_orchestrator->set_level_config(*benchmark_config);

        ml::s7::FLevelDefinitionReader reader;
        auto const script_path{
            FPaths::Combine(FPaths::ProjectDir(), ml::frame_memory_level_benchmark::script_path)};
        auto const scripted_definition{reader.read_file(script_path)};
        if (!checks_.is_true(static_cast<bool>(scripted_definition),
                             TEXT("Benchmark Scheme level produces a valid definition"))) {
            return;
        }

        ml::FLevelLoader loader{*benchmark_orchestrator};
        auto const load_result{loader.load(scripted_definition.definition.GetValue())};
        if (!checks_.is_true(static_cast<bool>(load_result),
                             TEXT("Benchmark Scheme level loads"))) {
            return;
        }

        auto& world{level_setup_.get_world()};
        auto driver{ml::TestSimulationDriver::from_world(world)};
        auto& orchestrator{driver.orchestrator};

        if (orchestrator.get_state() == EOrchestratorState::Running) {
            orchestrator.pause_simulation();
        }
        driver.set_time_scale(ml::frame_memory_level_benchmark::benchmark_time_scale);
        orchestrator.set_end_tick_test_hook(
            FOrchestratorEndTickTestHook::CreateRaw(this, &FrameMemoryLevelBenchmark::sample_tick));
        orchestrator.start_simulation();

        auto* const simulation{orchestrator.get_level_simulation()};
        checks_.is_true(simulation != nullptr, TEXT("Benchmark level simulation is available"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks_);

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

        int32 ticks_per_advance_argument{100};
        FParse::Value(FCommandLine::Get(),
                      TEXT("SandboxFrameMemoryLevelBenchmarkTicksPerAdvance="),
                      ticks_per_advance_argument);
        auto const ticks_per_advance{
            static_cast<uint64>(FMath::Max(ticks_per_advance_argument, 1))};

        auto const started_at{FPlatformTime::Seconds()};
        uint64 presentation_updates{};
        while (orchestrator.get_completed_ticks() - start_tick < ticks_to_run &&
               orchestrator.get_state() == EOrchestratorState::Running) {
            auto const completed_ticks{orchestrator.get_completed_ticks() - start_tick};
            auto const batch_ticks{FMath::Min(ticks_per_advance, ticks_to_run - completed_ticks)};
            auto const unscaled_dt{static_cast<double>(batch_ticks) * tick_period /
                                   ml::frame_memory_level_benchmark::benchmark_time_scale};
            orchestrator.tick(unscaled_dt);
            ++presentation_updates;
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
        for (auto const& sample : samples_.values()) {
            peak_claimed_bytes = FMath::Max(peak_claimed_bytes, sample.claimed_bytes);
            peak_payload_bytes = FMath::Max(peak_payload_bytes, sample.payload_bytes);
            total_padding_bytes += sample.padding_bytes;
            total_root_claims += sample.root_claim_count;
            overflow_count = FMath::Max(overflow_count, sample.overflow_count);
            peak_fighters = FMath::Max(peak_fighters, sample.fighter_count);
        }

        auto const completed_ticks{orchestrator.get_completed_ticks() - start_tick};
        auto const ticks_per_second{static_cast<double>(completed_ticks) / elapsed_seconds};
        auto const mean_tick_microseconds{elapsed_seconds * 1'000'000.0 /
                                          static_cast<double>(completed_ticks)};
        TestRunner->AddInfo(FString::Printf(
            TEXT("Frame memory level benchmark: script=%s, "
                 "time_scale=%.1f, "
                 "initial_capitals=%d, final_capitals=%d, initial_turrets=%d, final_turrets=%d, "
                 "simulated_seconds=%.3f, ticks=%llu, "
                 "ticks_per_advance=%llu, presentation_updates=%llu, "
                 "elapsed_seconds=%.6f, ticks_per_second=%.3f, "
                 "mean_tick_us=%.3f, "
                 "peak_claimed_bytes=%llu, peak_payload_bytes=%llu, "
                 "total_padding_bytes=%llu, total_root_claims=%llu, peak_fighters=%d, "
                 "lasers_spawned=%d"),
            *script_path,
            ml::frame_memory_level_benchmark::benchmark_time_scale,
            initial_capitals,
            driver.get_capital_ships().get_num_instances(),
            initial_turrets,
            orchestrator.get_turrets()->get_num_instances(),
            static_cast<double>(completed_ticks) * tick_period,
            completed_ticks,
            ticks_per_advance,
            presentation_updates,
            elapsed_seconds,
            ticks_per_second,
            mean_tick_microseconds,
            static_cast<uint64>(peak_claimed_bytes),
            static_cast<uint64>(peak_payload_bytes),
            static_cast<uint64>(total_padding_bytes),
            total_root_claims,
            peak_fighters,
            orchestrator.get_lasers()->get_number_spawned()));

        checks_.are_equal(static_cast<uint64>(ticks_to_run),
                          completed_ticks,
                          TEXT("Benchmark completes the requested simulation duration"));
        checks_.are_equal(uint64{0}, overflow_count, TEXT("Frame memory never overflows"));
        checks_.is_true(peak_claimed_bytes > 0, TEXT("Simulation scratch uses frame memory"));
        checks_.is_true(peak_fighters > 0, TEXT("Capitals launch fighters"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks_);
    }
};

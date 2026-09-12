#include "test_batch_orchestrator_reset_scenario.h"
#include "test_batch_orchestrator_setup_scenario.h"
#include "test_capital_ship_proxy_scenario.h"
#include "test_entity_interface_scenario.h"
#include "test_hud_manager_scenario.h"
#include "test_level_loader_scenario.h"
#include "test_player_ship_death_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SpaceGameTestSettings.h>
#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <CQTest.h>
#include <HAL/PlatformTime.h>
#include <Misc/Paths.h>

#define SHARED_SIMULATION_TEST(METHOD_NAME, SCENARIO_TYPE, ...) \
    TEST_METHOD(METHOD_NAME)                                    \
    { run_scenario<SCENARIO_TYPE>(__VA_ARGS__); }

TEST_CLASS(SharedSimulation, "Sandbox.LevelTests")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};
    TUniquePtr<ml::FSimulationTestContext> context{nullptr};
    TUniquePtr<ml::FSimulationTestScenario> scenario{nullptr};
    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    {
        if (scenario) {
            scenario->tear_down();
        }
        scenario.Reset();
        context.Reset();
        level_setup.end_test();
    }

    AFTER_ALL()
    { level_setup.teardown(); }
  private:
    template <typename T, typename... Args>
    void run_scenario(Args && ... args) {
        auto* const orchestrator{level_setup.get_orchestrator()};
        if (!checks.is_valid(orchestrator, TEXT("Shared simulation orchestrator is available"))) {
            return;
        }

        context = MakeUnique<ml::FSimulationTestContext>(ml::FSimulationTestContext{
            .orchestrator = *orchestrator,
            .automation_test = *TestRunner,
            .command_builder = TestCommandBuilder,
            .checks = checks,
            .config = level_setup.get_config(),
            .world = level_setup.get_world(),
            .level_construction_count = level_setup.get_construction_count(),
        });
        scenario = MakeUnique<T>(*context, Forward<Args>(args)...);
        check(scenario);
        scenario->run();
    }
  public:
    SHARED_SIMULATION_TEST(Orchestrator_SpawnMissingActors,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SpawnMissingActors)
    SHARED_SIMULATION_TEST(Orchestrator_SimulationClockConversions,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::SimulationClockConversions)
    SHARED_SIMULATION_TEST(Orchestrator_LevelTelemetry,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::LevelTelemetry)
    SHARED_SIMULATION_TEST(Orchestrator_PresentationFrameOrdering,
                           ml::FTestBatchOrchestratorSetupScenario,
                           ml::EOrchestratorSetupScenario::PresentationFrameOrdering)
    SHARED_SIMULATION_TEST(Orchestrator_ResetForNewLevel, ml::FTestBatchOrchestratorResetScenario)
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesDefinition, ml::FLevelLoaderScenario)
    SHARED_SIMULATION_TEST(LevelLoader_MaterialisesPlayerlessCameraDefinition,
                           ml::FLevelLoaderCameraScenario)

    SHARED_SIMULATION_TEST(CapitalShipProxy_HealthOverridesConfig,
                           ml::FTestCapitalShipProxyScenario)
    SHARED_SIMULATION_TEST(EntityInterface_ConvertsProxiesAndResolvesTargets,
                           ml::FEntityInterfaceScenario)
    SHARED_SIMULATION_TEST(HUD_LateRegistrationSynchronisesAndUnregisters,
                           ml::FTestHUDManagerScenario,
                           ml::EHUDManagerScenario::LateHUDRegistrationSynchronisesAndUnregisters)
    SHARED_SIMULATION_TEST(PlayerShip_LethalDamageDestroysPlayerShip,
                           ml::FTestPlayerShipDeathScenario)
};

#undef SHARED_SIMULATION_TEST

TEST_CLASS(TelemetryBenchmark, "SandboxBenchmarks.TelemetryBenchmark")
{
    inline static ml::FTestBatchOrchestratorLevelSetup level_setup{};

    ml::FSoftTestAssertions checks{};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
    }

    AFTER_EACH()
    { level_setup.end_test(); }

    AFTER_ALL()
    { level_setup.teardown(); }

    TEST_METHOD(DetailedTimingThroughput)
    {
        TestCommandBuilder.Do([this] {
            auto* const orchestrator{level_setup.get_orchestrator()};
            if (!checks.is_valid(orchestrator, TEXT("Telemetry benchmark orchestrator is valid"))) {
                return;
            }

            ml::s7::FLevelDefinitionReader reader;
            auto const script_path{FPaths::Combine(
                FPaths::ProjectDir(), TEXT("LevelScripts"), TEXT("SparkRendererShowcase.scm"))};
            auto const scripted_definition{reader.read_file(script_path)};
            if (!checks.is_true(static_cast<bool>(scripted_definition),
                                TEXT("Telemetry benchmark battle loads"))) {
                return;
            }

            constexpr auto simulated_seconds{300.0};
            constexpr auto frame_seconds{1.0 / 60.0};
            constexpr auto time_scale{100.0};
            constexpr auto measured_pairs{7};
            TArray<double> timing_off_throughput;
            TArray<double> timing_on_throughput;
            timing_off_throughput.Reserve(measured_pairs);
            timing_on_throughput.Reserve(measured_pairs);

            auto run_trial = [&](bool const detailed_timing) {
                orchestrator->reset_for_new_level();
                orchestrator->set_presentation_enabled(false);
                ml::FLevelLoader loader{*orchestrator};
                auto const load_result{loader.load(scripted_definition.definition.GetValue())};
                if (!checks.is_true(static_cast<bool>(load_result),
                                    TEXT("Telemetry benchmark trial loads"))) {
                    return 0.0;
                }
                orchestrator->start_simulation();
                orchestrator->set_time_scale(time_scale);

                auto& telemetry{orchestrator->get_level_telemetry_manager()};
                FLevelTelemetryRunMetadata metadata;
                metadata.level_id = TEXT("spark-renderer-showcase");
                metadata.requested_duration_seconds = simulated_seconds;
                metadata.initial_requested_time_scale = time_scale;
                metadata.detailed_timing = detailed_timing;
                telemetry.begin_run(MoveTemp(metadata));

                auto const started_at{FPlatformTime::Seconds()};
                while (orchestrator->get_simulation_time() < simulated_seconds) {
                    orchestrator->tick(frame_seconds);
                }
                auto const elapsed_seconds{FPlatformTime::Seconds() - started_at};
                telemetry.finalize_completed(ELevelTelemetryRunEndReason::DurationReached);
                auto const history_stats{telemetry.get_history_stats()};
                auto const run_record{telemetry.take_finalized_run()};
                double telemetry_cpu_ms{};
                double simulation_cpu_ms{};
                if (run_record.IsSet()) {
                    for (auto const& window : run_record->performance_windows) {
                        auto const& telemetry_timing{window.systems[static_cast<int32>(
                            ESimulationTelemetryTimingSystem::Telemetry)]};
                        telemetry_cpu_ms +=
                            telemetry_timing.mean_ms * telemetry_timing.sample_count;
                        simulation_cpu_ms +=
                            window.simulation_tick.mean_ms * window.simulation_tick.sample_count;
                    }
                }
                auto const telemetry_cpu_percent{
                    simulation_cpu_ms > 0.0 ? telemetry_cpu_ms / simulation_cpu_ms * 100.0 : 0.0};
                TestRunner->AddInfo(FString::Printf(
                    TEXT("Telemetry integration: detailed=%d telemetry_cpu_ms=%.6f "
                         "simulation_cpu_ms=%.6f telemetry_cpu_percent=%.6f rows=%d "
                         "payload_writes=%llu acquired_blocks=%d retained_blocks=%d "
                         "allocated_bytes=%llu "
                         "manager_size=%llu"),
                    detailed_timing ? 1 : 0,
                    telemetry_cpu_ms,
                    simulation_cpu_ms,
                    telemetry_cpu_percent,
                    history_stats.used_sample_count,
                    history_stats.payload_write_count,
                    history_stats.acquired_block_count,
                    history_stats.retained_block_count,
                    history_stats.total_byte_capacity,
                    sizeof(FLevelTelemetryManager)));
                return static_cast<double>(orchestrator->get_completed_ticks()) / elapsed_seconds;
            };

            run_trial(false);
            run_trial(true);
            for (int32 pair_index{0}; pair_index < measured_pairs; ++pair_index) {
                if ((pair_index & 1) == 0) {
                    timing_off_throughput.Add(run_trial(false));
                    timing_on_throughput.Add(run_trial(true));
                } else {
                    timing_on_throughput.Add(run_trial(true));
                    timing_off_throughput.Add(run_trial(false));
                }
            }

            timing_off_throughput.Sort();
            timing_on_throughput.Sort();
            auto const median_index{measured_pairs / 2};
            auto const timing_off_median{timing_off_throughput[median_index]};
            auto const timing_on_median{timing_on_throughput[median_index]};
            auto const throughput_impact_percent{(timing_off_median - timing_on_median) /
                                                 timing_off_median * 100.0};
            TestRunner->AddInfo(FString::Printf(
                TEXT("Telemetry detailed timing A/B: battle=spark-renderer-showcase, "
                     "presentation=simulation_only, duration=%.0fs, time_scale=%.0fx, pairs=%d, "
                     "off_median=%.3f ticks/s, on_median=%.3f ticks/s, impact=%.3f%%"),
                simulated_seconds,
                time_scale,
                measured_pairs,
                timing_off_median,
                timing_on_median,
                throughput_impact_percent));
            TestRunner->TestTrue(TEXT("Detailed timing median throughput impact is below 2%"),
                                 throughput_impact_percent < 2.0);
        });
    }
};

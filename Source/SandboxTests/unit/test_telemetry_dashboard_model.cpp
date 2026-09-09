#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <CQTest.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>

namespace {
auto make_catalog_record(FString run_id, FString level, FString launch) -> FLevelTelemetryReport {
    FLevelTelemetryReport record;
    record.metadata.run_id = MoveTemp(run_id);
    record.metadata.map_name = level;
    record.metadata.level_id = FName{level};
    record.metadata.level_display_name = MoveTemp(level);
    record.metadata.launched_utc = MoveTemp(launch);
    record.metadata.tick_rate_hz = 60.0;
    record.metadata.tick_period_seconds = 1.0 / 60.0;
    record.completion.completed_utc = record.metadata.launched_utc;
    return record;
}
}

TEST_CLASS(TelemetryDashboardAnalysis, "Sandbox.UnitTests")
{
    TEST_METHOD(DefinesEveryDashboardMetric)
    {
        auto const analysis{analyze_level_telemetry_run({})};
        TestRunner->TestEqual(TEXT("Every dashboard metric has a series"),
                              analysis.metrics.Num(),
                              static_cast<int32>(ETelemetryDashboardMetric::COUNT));
        for (uint8 value{}; value < static_cast<uint8>(ETelemetryDashboardMetric::COUNT); ++value) {
            auto const metric{static_cast<ETelemetryDashboardMetric>(value)};
            TestRunner->TestFalse(TEXT("Every dashboard metric has a title"),
                                  telemetry_metric_title(metric).IsEmpty());
            TestRunner->TestFalse(TEXT("Every dashboard metric has units"),
                                  telemetry_metric_units(metric).IsEmpty());
        }
    }

    TEST_METHOD(DerivesWeightedIntervalMetrics)
    {
        FLevelTelemetryReport record;
        record.loaded_schema_version = 1;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(0.5, uint64{90});
        record.completed_ticks_by_real_time.add(1.5, uint64{150});
        record.completed_ticks_by_real_time.add(3.5, uint64{270});
        record.tick_series.requested_time_scale.add(0, 2.0);
        record.tick_series.spawned_entities.add(0, 0);
        record.tick_series.spawned_entities.add(90, 10);
        record.tick_series.spawned_entities.add(150, 20);
        record.tick_series.spawned_entities.add(270, 40);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* observed{analysis.find_metric(ETelemetryDashboardMetric::ObservedTimeScale)};
        auto const* requested_ratio{
            analysis.find_metric(ETelemetryDashboardMetric::RequestedTimeScaleRatio)};
        auto const* spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        TestRunner->TestNotNull(TEXT("Observed metric exists"), observed);
        TestRunner->TestEqual(TEXT("Both post-warm-up intervals are analyzed"),
                              observed ? observed->values.Num() : 0,
                              2);
        TestRunner->TestTrue(TEXT("Observed time scale uses tick period"),
                             observed && FMath::IsNearlyEqual(observed->values[0], 1.0f));
        TestRunner->TestTrue(TEXT("Requested-scale ratio divides by the stable requested scale"),
                             requested_ratio &&
                                 FMath::IsNearlyEqual(requested_ratio->values[0], 50.0f));
        TestRunner->TestTrue(TEXT("Counter rates use endpoint deltas over real time"),
                             spawn_rate && FMath::IsNearlyEqual(spawn_rate->values[0], 10.0f) &&
                                 FMath::IsNearlyEqual(spawn_rate->values[1], 10.0f));
        TestRunner->TestTrue(TEXT("Weighted summary uses real durations"),
                             observed && observed->weighted_mean.IsSet() &&
                                 FMath::IsNearlyEqual(observed->weighted_mean.GetValue(), 1.0));
    }

    TEST_METHOD(DerivesCurrentWorkloadAndRatesBySimulatedTime)
    {
        FLevelTelemetryReport record;
        FLevelTelemetryBattleSample begin;
        begin.simulated_elapsed_seconds = 0.0;
        begin.alive[0][0] = 2;
        begin.combat.spawned[0][0] = 2;
        begin.active_lasers = 3;
        begin.lasers_fired = 10;
        begin.range_query_count = 20;
        FLevelTelemetryBattleSample end{begin};
        end.completed_tick = 60;
        end.simulated_elapsed_seconds = 1.0;
        end.alive[0][0] = 3;
        end.combat.spawned[0][0] = 5;
        end.active_lasers = 4;
        end.lasers_fired = 15;
        end.range_query_count = 27;
        record.battle_samples = {begin, end};

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* const active{analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        auto const* const spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        auto const* const laser_rate{
            analysis.find_metric(ETelemetryDashboardMetric::LaserFireRate)};
        auto const* const query_rate{
            analysis.find_metric(ETelemetryDashboardMetric::RangeQueryRate)};
        TestRunner->TestTrue(TEXT("Current workload uses simulated coordinates"),
                             active && active->uses_simulated_time &&
                                 active->real_elapsed_seconds == TArray<float>{1.0f} &&
                                 active->values == TArray<float>{3.0f});
        TestRunner->TestTrue(TEXT("Current combat rates use simulated durations"),
                             spawn_rate && spawn_rate->uses_simulated_time &&
                                 spawn_rate->values == TArray<float>{3.0f} && laser_rate &&
                                 laser_rate->values == TArray<float>{5.0f});
        TestRunner->TestTrue(TEXT("Current query rates use simulated durations"),
                             query_rate && query_rate->uses_simulated_time &&
                                 query_rate->values == TArray<float>{7.0f});
    }

    TEST_METHOD(ExcludesUnstableRequestedRatioAndRegressingCounters)
    {
        FLevelTelemetryReport record;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(0.5, uint64{30});
        record.completed_ticks_by_real_time.add(2.5, uint64{150});
        record.tick_series.requested_time_scale.add(0, 1.0);
        record.tick_series.requested_time_scale.add(90, 2.0);
        record.tick_series.active_entities.add(30, 7);
        record.tick_series.spawned_entities.add(30, 10);
        record.tick_series.spawned_entities.add(150, 5);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* requested_ratio{
            analysis.find_metric(ETelemetryDashboardMetric::RequestedTimeScaleRatio)};
        auto const* active{analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        auto const* spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        TestRunner->TestTrue(TEXT("Scale changes within an interval exclude the requested ratio"),
                             requested_ratio && requested_ratio->values.IsEmpty());
        TestRunner->TestTrue(TEXT("Gauges use sparse as-of values at the interval endpoint"),
                             active && active->values.Num() == 1 && active->values[0] == 7.0f);
        TestRunner->TestTrue(TEXT("Counter regressions exclude the affected rate"),
                             spawn_rate && spawn_rate->values.IsEmpty());
        TestRunner->TestEqual(TEXT("Requested throughput uses the endpoint as-of value"),
                              analysis.requested_time_scale[0],
                              2.0f);
    }

    TEST_METHOD(RequestedScaleRatioIsNotCappedAtOneHundredPercent)
    {
        FLevelTelemetryReport record;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(0.5, uint64{30});
        record.completed_ticks_by_real_time.add(1.5, uint64{120});
        record.tick_series.requested_time_scale.add(0, 1.0);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* const requested_ratio{
            analysis.find_metric(ETelemetryDashboardMetric::RequestedTimeScaleRatio)};
        TestRunner->TestTrue(TEXT("Running faster than requested produces a ratio above 100%"),
                             requested_ratio && requested_ratio->values.Num() == 1 &&
                                 FMath::IsNearlyEqual(requested_ratio->values[0], 150.0f));
        TestRunner->TestTrue(
            TEXT("The weighted mean retains the uncapped ratio"),
            requested_ratio && requested_ratio->weighted_mean.IsSet() &&
                FMath::IsNearlyEqual(requested_ratio->weighted_mean.GetValue(), 150.0));
    }

    TEST_METHOD(ExcludesTheFirstRealtimeIntervalAsWarmup)
    {
        FLevelTelemetryReport record;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(0.8, uint64{60});
        record.completed_ticks_by_real_time.add(1.8, uint64{120});
        record.tick_series.requested_time_scale.add(0, 1.0);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* const observed{
            analysis.find_metric(ETelemetryDashboardMetric::ObservedTimeScale)};
        auto const* const requested_ratio{
            analysis.find_metric(ETelemetryDashboardMetric::RequestedTimeScaleRatio)};
        TestRunner->TestTrue(TEXT("Only the post-warm-up interval is retained"),
                             observed && observed->values.Num() == 1 &&
                                 FMath::IsNearlyEqual(observed->values[0], 1.0f));
        TestRunner->TestTrue(
            TEXT("The weighted mean excludes the warm-up interval"),
            requested_ratio && requested_ratio->weighted_mean.IsSet() &&
                FMath::IsNearlyEqual(requested_ratio->weighted_mean.GetValue(), 100.0));
        TestRunner->TestTrue(
            TEXT("The throughput graph excludes the warm-up interval"),
            analysis.throughput_real_elapsed_seconds.Num() == 1 &&
                FMath::IsNearlyEqual(analysis.throughput_real_elapsed_seconds[0], 1.8f));
    }
};

TEST_CLASS(TelemetryRunCatalog, "Sandbox.UnitTests")
{
    TEST_METHOD(RefreshesFiltersPreservesSelectionAndReportsCorruption)
    {
        auto const directory{FPaths::Combine(FPaths::ProjectSavedDir(),
                                             TEXT("Automation"),
                                             TEXT("TelemetryCatalog"),
                                             FGuid::NewGuid().ToString())};
        ON_SCOPE_EXIT {
            IFileManager::Get().DeleteDirectory(*directory, false, true);
        };
        auto older{make_catalog_record(TEXT("older"), TEXT("Alpha"), TEXT("2026-01-01T00:00:00Z"))};
        auto newer{make_catalog_record(TEXT("newer"), TEXT("Beta"), TEXT("2026-02-01T00:00:00Z"))};
        TestRunner->TestTrue(TEXT("Older fixture writes"),
                             write_level_telemetry_run(older, directory).has_value());
        TestRunner->TestTrue(TEXT("Newer fixture writes"),
                             write_level_telemetry_run(newer, directory).has_value());
        FFileHelper::SaveStringToFile(TEXT("not json"),
                                      *FPaths::Combine(directory, TEXT("broken.json")));
        auto invalid_reason_json{serialize_level_telemetry_run(older)};
        invalid_reason_json.ReplaceInline(TEXT("world_end"), TEXT("future_reason"));
        FFileHelper::SaveStringToFile(invalid_reason_json,
                                      *FPaths::Combine(directory, TEXT("invalid_reason.json")));

        FTelemetryRunCatalog catalog{directory};
        catalog.refresh();
        TestRunner->TestEqual(TEXT("Corrupt and incompatible files are counted"),
                              catalog.get_unreadable_file_count(),
                              2);
        TestRunner->TestEqual(TEXT("Valid files are catalogued"), catalog.get_runs().Num(), 2);
        TestRunner->TestEqual(
            TEXT("Runs sort newest first"), catalog.get_runs()[0].run_id, FString{TEXT("newer")});
        TestRunner->TestTrue(TEXT("Selected full record is loaded"),
                             catalog.get_selected_record() != nullptr);
        catalog.select_run(TEXT("older"));
        catalog.refresh();
        TestRunner->TestEqual(TEXT("Refresh preserves a valid selection"),
                              catalog.get_selected_run_id(),
                              FString{TEXT("older")});
        catalog.set_level_filter(TEXT("Beta"));
        TestRunner->TestEqual(TEXT("Level filters restrict runs"), catalog.get_runs().Num(), 1);
        TestRunner->TestEqual(TEXT("Filtering selects the first matching run"),
                              catalog.get_selected_run_id(),
                              FString{TEXT("newer")});
    }
};

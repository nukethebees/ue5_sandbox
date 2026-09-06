#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <CQTest.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>

namespace {
auto make_catalog_record(FString run_id, FString level, FString launch)
    -> FLevelTelemetryRunRecord {
    FLevelTelemetryRunRecord record;
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
    TEST_METHOD(DerivesWeightedIntervalMetrics)
    {
        FLevelTelemetryRunRecord record;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(1.0, uint64{60});
        record.completed_ticks_by_real_time.add(3.0, uint64{180});
        record.tick_series.requested_time_scale.add(0, 2.0);
        record.tick_series.spawned_entities.add(0, 0);
        record.tick_series.spawned_entities.add(60, 10);
        record.tick_series.spawned_entities.add(180, 30);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* observed{analysis.find_metric(ETelemetryDashboardMetric::ObservedTimeScale)};
        auto const* achievement{
            analysis.find_metric(ETelemetryDashboardMetric::TimeScaleAchievement)};
        auto const* spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        TestRunner->TestNotNull(TEXT("Observed metric exists"), observed);
        TestRunner->TestEqual(
            TEXT("Both adjacent intervals are analyzed"), observed ? observed->values.Num() : 0, 2);
        TestRunner->TestTrue(TEXT("Observed time scale uses tick period"),
                             observed && FMath::IsNearlyEqual(observed->values[0], 1.0f));
        TestRunner->TestTrue(TEXT("Achievement divides by stable requested scale"),
                             achievement && FMath::IsNearlyEqual(achievement->values[0], 50.0f));
        TestRunner->TestTrue(TEXT("Counter rates use endpoint deltas over real time"),
                             spawn_rate && FMath::IsNearlyEqual(spawn_rate->values[0], 10.0f) &&
                                 FMath::IsNearlyEqual(spawn_rate->values[1], 10.0f));
        TestRunner->TestTrue(TEXT("Weighted summary uses real durations"),
                             observed && observed->weighted_mean.IsSet() &&
                                 FMath::IsNearlyEqual(observed->weighted_mean.GetValue(), 1.0));
    }

    TEST_METHOD(ExcludesUnstableAchievementAndRegressingCounters)
    {
        FLevelTelemetryRunRecord record;
        record.metadata.tick_period_seconds = 1.0 / 60.0;
        record.completed_ticks_by_real_time.add(0.0, uint64{0});
        record.completed_ticks_by_real_time.add(2.0, uint64{120});
        record.tick_series.requested_time_scale.add(0, 1.0);
        record.tick_series.requested_time_scale.add(60, 2.0);
        record.tick_series.active_entities.add(30, 7);
        record.tick_series.spawned_entities.add(0, 10);
        record.tick_series.spawned_entities.add(120, 5);

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* achievement{
            analysis.find_metric(ETelemetryDashboardMetric::TimeScaleAchievement)};
        auto const* active{analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        auto const* spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        TestRunner->TestTrue(TEXT("Scale changes within an interval exclude achievement"),
                             achievement && achievement->values.IsEmpty());
        TestRunner->TestTrue(TEXT("Gauges use sparse as-of values at the interval endpoint"),
                             active && active->values.Num() == 1 && active->values[0] == 7.0f);
        TestRunner->TestTrue(TEXT("Counter regressions exclude the affected rate"),
                             spawn_rate && spawn_rate->values.IsEmpty());
        TestRunner->TestEqual(TEXT("Requested throughput uses the endpoint as-of value"),
                              analysis.requested_time_scale[0],
                              2.0f);
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

        FTelemetryRunCatalog catalog{directory};
        catalog.refresh();
        TestRunner->TestEqual(
            TEXT("Corrupt files are counted"), catalog.get_unreadable_file_count(), 1);
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

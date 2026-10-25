#include <ioj/sim/telemetry/report_json.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <SandboxCoreEngine/strings.h>

#include <CQTest.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>

namespace {
auto make_catalog_record(FString run_id, FString level, FString launch)
    -> ioj::sim::LevelTelemetryReport {
    ioj::sim::LevelTelemetryReport record;
    record.run.metadata.run_id = TCHAR_TO_UTF8(*run_id);
    record.run.metadata.map_name = TCHAR_TO_UTF8(*level);
    record.run.metadata.level_id = TCHAR_TO_UTF8(*level);
    record.run.metadata.level_display_name = TCHAR_TO_UTF8(*level);
    record.run.metadata.launched_utc = TCHAR_TO_UTF8(*launch);
    record.run.completion.completed_utc = record.run.metadata.launched_utc;
    return record;
}
}

TEST_CLASS(TelemetryDashboardAnalysis, "Sandbox.UnitTests")
{
    TEST_METHOD(DefinesEveryDashboardMetric)
    {
        auto const analysis{analyze_level_telemetry_run({})};
        TestRunner->TestEqual(TEXT("Every battle metric has a series"),
                              analysis.metrics.Num(),
                              static_cast<int32>(ETelemetryDashboardMetric::Count));
        constexpr auto metric_count{static_cast<uint8>(ETelemetryDashboardMetric::Count)};
        for (uint8 value{}; value < metric_count; ++value) {
            auto const metric{static_cast<ETelemetryDashboardMetric>(value)};
            TestRunner->TestFalse(TEXT("Every metric has a title"),
                                  telemetry_metric_title(metric).IsEmpty());
            TestRunner->TestFalse(TEXT("Every metric has units"),
                                  telemetry_metric_units(metric).IsEmpty());
        }
    }

    TEST_METHOD(MissingBattleSamplesDoNotInventMetrics)
    {
        ioj::sim::LevelTelemetryReport record;
        record.run.completion.completed_ticks = 270;
        record.run.tick_series.spawned_entities.add(0, 0);
        record.run.tick_series.spawned_entities.add(90, 10);
        record.run.tick_series.spawned_entities.add(150, 20);
        record.run.tick_series.spawned_entities.add(270, 40);

        auto const analysis{analyze_level_telemetry_run(record)};
        for (auto const& metric : analysis.metrics) {
            TestRunner->TestTrue(TEXT("Sparse counters do not reconstruct missing battle samples"),
                                 metric.values.IsEmpty() && !metric.weighted_mean.IsSet());
        }
        TestRunner->TestTrue(TEXT("Missing combat history remains unavailable"),
                             analysis.battle_shots.IsEmpty());
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
        auto const serialized{ioj::sim::serialize_level_telemetry_report(older)};
        if (!TestRunner->TestTrue(TEXT("Catalog fixture serialization succeeds"),
                                  serialized.has_value())) {
            return;
        }
        auto invalid_reason_json{ioj::to_fstring(*serialized)};
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

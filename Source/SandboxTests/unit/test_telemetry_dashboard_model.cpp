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
                              static_cast<int32>(ETelemetryDashboardMetric::COUNT));
        for (uint8 value{}; value < static_cast<uint8>(ETelemetryDashboardMetric::COUNT); ++value) {
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

    TEST_METHOD(DerivesBattleForcesCombatAndProjectileRates)
    {
        ioj::sim::LevelTelemetryReport record;
        ::ioj::sim::LevelTelemetryBattleSample begin;
        begin.alive[0][0] = 2;
        begin.combat.spawned[0][0] = 2;
        begin.active_lasers = 3;
        begin.lasers_fired = 10;
        ::ioj::sim::LevelTelemetryBattleSample end{begin};
        end.completed_tick = 60;
        end.alive[0][0] = 3;
        end.combat.spawned[0][0] = 5;
        end.combat.shots[0][0] = 5;
        end.combat.hits[0][0] = 2;
        end.combat.damage_dealt[0][0] = 50;
        end.active_lasers = 4;
        end.lasers_fired = 15;
        record.run.battle_samples = {begin, end};

        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* active{analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        auto const* spawn_rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        auto const* laser_rate{analysis.find_metric(ETelemetryDashboardMetric::LaserFireRate)};
        TestRunner->TestTrue(TEXT("Force metrics use simulated coordinates"),
                             active && active->simulated_elapsed_seconds == TArray<float>{1.f} &&
                                 active->values == TArray<float>{3.f});
        TestRunner->TestTrue(TEXT("Combat rates use simulated durations"),
                             spawn_rate && spawn_rate->values == TArray<float>{3.f} && laser_rate &&
                                 laser_rate->values == TArray<float>{5.f});
        TestRunner->TestTrue(TEXT("Combat graph retains shots, hits and damage"),
                             analysis.battle_shots == TArray<float>{0.f, 5.f} &&
                                 analysis.battle_hits == TArray<float>{0.f, 2.f} &&
                                 analysis.battle_damage_dealt == TArray<float>{0.f, 50.f});

        auto baseline{record};
        baseline.run.battle_samples[1].alive[0][0] = 2;
        auto const baseline_analysis{analyze_level_telemetry_run(baseline)};
        auto const* baseline_active{
            baseline_analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        TestRunner->TestTrue(
            TEXT("Battle baselines align by simulated time and retain distinct forces"),
            baseline_active && active &&
                baseline_active->simulated_elapsed_seconds == active->simulated_elapsed_seconds &&
                baseline_active->values == TArray<float>{2.f});
    }

    TEST_METHOD(SampledGaugesSurviveRegressingCounterIntervals)
    {
        ioj::sim::LevelTelemetryReport record;
        ioj::sim::LevelTelemetryBattleSample begin;
        begin.completed_tick = 30;
        begin.alive[0][0] = 7;
        begin.combat.spawned[0][0] = 10;
        auto end{begin};
        end.completed_tick = 150;
        end.combat.spawned[0][0] = 5;
        record.run.battle_samples = {begin, end};
        auto const analysis{analyze_level_telemetry_run(record)};
        auto const* active{analysis.find_metric(ETelemetryDashboardMetric::ActiveEntities)};
        auto const* rate{analysis.find_metric(ETelemetryDashboardMetric::SpawnRate)};
        TestRunner->TestTrue(TEXT("Gauges retain sampled endpoint values"),
                             active && active->values == TArray<float>{7.f});
        TestRunner->TestTrue(TEXT("Regressing counters do not produce negative rates"),
                             rate && rate->values.IsEmpty());
    }

    TEST_METHOD(EmptyAndSingleSampleRunsDoNotInventIntervals)
    {
        ioj::sim::LevelTelemetryReport record;
        for (int32 sample_count{}; sample_count < 2; ++sample_count) {
            auto const analysis{analyze_level_telemetry_run(record)};
            for (auto const& metric : analysis.metrics) {
                TestRunner->TestTrue(TEXT("No interval means no rate or weighted summary"),
                                     metric.values.IsEmpty() && !metric.weighted_mean.IsSet());
            }
            record.run.battle_samples.push_back({});
        }
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

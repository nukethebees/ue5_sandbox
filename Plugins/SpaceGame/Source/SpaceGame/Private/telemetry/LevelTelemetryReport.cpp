#include <SpaceGame/telemetry/LevelTelemetryReport.h>

FLevelTelemetryReport::FLevelTelemetryReport(FLevelTelemetryRunRecord record)
    : loaded_schema_version{record.loaded_schema_version}
    , completion{MoveTemp(record.completion)}
    , tick_series{MoveTemp(record.tick_series)}
    , completed_ticks_by_real_time{MoveTemp(record.completed_ticks_by_real_time)}
    , battle_samples{MoveTemp(record.battle_samples)} {
    static_cast<FLevelTelemetryRunMetadata&>(metadata) = MoveTemp(record.metadata);
    performance_windows.Reserve(record.performance_windows.Num());
    for (auto const& source : record.performance_windows) {
        FLevelTelemetryPerformanceWindow window;
        window.real_elapsed_seconds = source.real_elapsed_seconds;
        window.completed_tick = source.completed_tick;
        window.frame = source.frame;
        window.game_thread = source.game_thread;
        window.simulation_tick = source.simulation_tick;
        window.phases = source.phases;
        window.phase_cpu_share = source.phase_cpu_share;
        auto const count{FSimulationTelemetryPerformanceWindow::system_count};
        for (int32 i{}; i < count; ++i) {
            auto const destination{
                i == static_cast<int32>(ESimulationTelemetryTimingSystem::Telemetry)
                    ? static_cast<int32>(ELevelTelemetryTimingSystem::Telemetry)
                    : i};
            window.systems[destination] = source.systems[i];
        }
        performance_windows.Add(MoveTemp(window));
    }
}
void append_external_timings(FLevelTelemetryReport& report,
                             TConstArrayView<FLevelExternalTimingSample> const samples) {
    auto const count{report.performance_windows.Num()};
    if (count == 0) {
        return;
    }
    TArray<double> values;
    int32 sample_begin{};
    auto const sample_count{samples.Num()};
    for (int32 index{}; index < count; ++index) {
        auto sample_end{sample_begin};
        while (sample_end < sample_count &&
               FMath::Min(samples[sample_end].window_index, count - 1) == index) {
            ++sample_end;
        }
        for (auto const system :
             {ELevelTelemetryTimingSystem::Hud, ELevelTelemetryTimingSystem::Presentation}) {
            values.Reset();
            double total{};
            for (auto sample_index{sample_begin}; sample_index < sample_end; ++sample_index) {
                auto const& sample{samples[sample_index]};
                if (sample.system == system) {
                    values.Add(sample.seconds * 1000.0);
                    total += sample.seconds * 1000.0;
                }
            }
            if (values.IsEmpty()) {
                continue;
            }
            values.Sort();
            auto& result{report.performance_windows[index].systems[static_cast<int32>(system)]};
            result.sample_count = values.Num();
            result.mean_ms = total / values.Num();
            result.max_ms = values.Last();
            result.p95_ms = values[FMath::Clamp(
                FMath::CeilToInt(values.Num() * 0.95) - 1, 0, values.Num() - 1)];
        }
        sample_begin = sample_end;
    }
}

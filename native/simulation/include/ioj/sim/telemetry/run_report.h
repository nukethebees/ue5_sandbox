#pragma once

#include <ioj/levels/launch_options.h>
#include <ioj/sim/telemetry/report.h>
#include <ioj/sim/telemetry/run_comparison.h>
#include <ioj/sim/telemetry/run_statistics.h>

#include <expected>
#include <filesystem>
#include <string>

namespace ioj::sim::telemetry {
[[nodiscard]] auto summarize_run(LevelTelemetryReport const& report) -> RunStatistics;
[[nodiscard]] auto make_run_report(LevelTelemetryRunRecord record,
                                   levels::LaunchOptions const& options,
                                   bool presentation_enabled) -> LevelTelemetryReport;
[[nodiscard]] auto compare_run_context(LevelTelemetryReport const& baseline,
                                       LevelTelemetryReport const& candidate) -> RunComparison;
auto read_run_report(std::filesystem::path const& path)
    -> std::expected<LevelTelemetryReport, std::string>;
}

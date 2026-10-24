#include <ioj/sim/telemetry/report.h>

#include <utility>

namespace ioj::sim {
LevelTelemetryReport::LevelTelemetryReport(LevelTelemetryRunRecord record)
    : run{std::move(record)} {}
}

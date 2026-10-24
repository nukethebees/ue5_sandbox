#pragma once

#include <ioj/sim/telemetry/report_environment.h>
#include <ioj/sim/telemetry/run_record.h>

namespace ioj::sim {
struct LevelTelemetryReport {
    LevelTelemetryReport() = default;
    explicit LevelTelemetryReport(LevelTelemetryRunRecord record);

    LevelTelemetryRunRecord run{};
    LevelTelemetryEnvironment environment{};
    bool started_paused{};
    bool opens_telemetry{};
    bool presentation_enabled{};
};
}

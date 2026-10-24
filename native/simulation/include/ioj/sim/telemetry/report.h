#pragma once

#include <ioj/sim/telemetry/report_environment.h>
#include <ioj/sim/telemetry/run_record.h>

namespace ioj::sim {
struct LevelTelemetryReport : LevelTelemetryRunRecord {
    LevelTelemetryReport() = default;
    explicit LevelTelemetryReport(LevelTelemetryRunRecord record);

    LevelTelemetryEnvironment environment{};
    std::string launch_state{"running"};
    std::string results_navigation{"none"};
    bool presentation_enabled{};
    std::string presentation_mode{"simulation_only"};
};
}

#pragma once

#include <ioj/sim/telemetry/run_record.h>

namespace ioj::sim::telemetry {
struct Report {
    Report() = default;
    explicit Report(RunRecord record);

    RunRecord run{};
    bool started_paused{};
    bool opens_telemetry{};
    bool presentation_enabled{};
};
}

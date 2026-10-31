#pragma once

#include <ioj/sim/telemetry/run_end_reason.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace ioj::sim::telemetry {
struct RunSummary {
    std::filesystem::path path{};
    std::string run_id{};
    std::string map_name{};
    std::string level_id{};
    std::string level_display_name{};
    std::string launched_utc{};
    LevelTelemetryRunEndReason completion_reason{LevelTelemetryRunEndReason::WorldEnd};

    [[nodiscard]] auto level_label() const -> std::string_view;
};
}

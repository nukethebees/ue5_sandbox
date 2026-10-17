#pragma once

#include <optional>
#include <string>

namespace ioj::sim {
struct LevelTelemetryRunMetadata {
    std::string run_id{};
    std::string map_name{};
    std::string level_id{};
    std::string level_display_name{};
    std::string launched_utc{};
    double tick_rate_hz{};
    double tick_period_seconds{};
    double initial_requested_time_scale{1.0};
    bool stop_when_battle_resolved{};
    std::string source_sha256{};
    std::optional<double> requested_duration_seconds{};
    double battle_sample_interval_seconds{1.0};
};
}

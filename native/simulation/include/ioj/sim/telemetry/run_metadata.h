#pragma once

#include <ioj/duration.h>

#include <optional>
#include <string>

namespace ioj::sim {
struct LevelTelemetryRunMetadata {
    std::string run_id{};
    std::string map_name{};
    std::string level_id{};
    std::string level_display_name{};
    std::string launched_utc{};
    bool stop_when_battle_resolved{};
    std::string level_source{};
    std::string settings_source{};
    std::optional<Duration> requested_duration_seconds{};
};
}

#pragma once

#include <ioj/duration.h>

#include <cstdint>
#include <optional>

namespace ioj::sim::telemetry {
struct RecordingConfig {
    bool enabled{true};
    // An explicit tick interval takes precedence. Time intervals round up to whole ticks.
    std::optional<std::uint64_t> interval_ticks{};
    Duration interval{Duration::make(1.0)};
    std::optional<Duration> duration{};
};
}

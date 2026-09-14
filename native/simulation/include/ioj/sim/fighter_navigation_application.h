#pragma once

#include "ioj/sim/index_span.h"
#include "ioj/sim/vectors3f.h"

#include <cstdint>
#include <span>

namespace ioj::sim::fighters {
struct NavigationScratch;
struct NavigationTelemetrySnapshot;

struct NavigationApplicationView {
    Vectors3fView movement_directions;
    Vectors3fConstView separation_steering;
    std::span<float const> float_biases;
    std::span<std::int8_t const> choices;
    std::span<std::uint8_t> risk_tiers;
    std::span<std::uint8_t> lower_risk_scan_counts;
    std::span<std::int16_t> periods;
    std::span<std::int16_t> remaining_ticks;
};

struct NavigationApplicationParameters {
    std::span<std::int16_t const> risk_periods;
    std::int8_t direct_choice;
    std::int8_t stop_choice;
    std::uint8_t scans_to_demote;
    float steering_zero_tolerance;
};

void apply_separation_steering(Vectors3fView movement_directions,
                               Vectors3fConstView separation_steering,
                               std::span<IndexSpan const> active_spans,
                               float separation_strength);

void apply_navigation_choices(NavigationApplicationView fighters,
                              std::span<IndexSpan const> active_spans,
                              NavigationApplicationParameters parameters,
                              NavigationScratch const& scratch,
                              NavigationTelemetrySnapshot& telemetry);
}

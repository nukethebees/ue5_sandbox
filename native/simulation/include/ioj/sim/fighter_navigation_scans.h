#pragma once

#include "ioj/sim/vectors3f.h"

#include <cstdint>
#include <span>

namespace ioj::sim::fighters {
struct NavigationScratch;

struct NavigationScanView {
    Vectors3fConstView locations;
    Vectors3fConstView preferred_directions;
    std::span<float const> move_distances;
    std::span<float const> speeds;
    std::span<float const> float_biases;
    std::span<std::uint32_t const> integral_biases;
    std::span<std::int8_t const> choices;
};

void prepare_preferred_navigation(NavigationScanView fighters,
                                  float lookahead_time,
                                  float minimum_lookahead_distance,
                                  float direction_zero_tolerance,
                                  NavigationScratch& scratch);

void resolve_preferred_navigation(NavigationScratch& scratch,
                                  std::span<std::int8_t> choices,
                                  std::span<std::uint8_t> clear_scan_counts,
                                  std::int8_t direct_choice,
                                  std::uint8_t clear_scans_to_end_avoidance);

void prepare_alternative_navigation(NavigationScanView fighters,
                                    float lookahead_time,
                                    float minimum_lookahead_distance,
                                    NavigationScratch& scratch);
}

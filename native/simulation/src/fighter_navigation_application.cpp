#include "ioj/sim/fighter_navigation_application.h"

#include "ioj/sim/fighter_navigation.h"
#include "ioj/sim/fighter_navigation_scratch.h"
#include "ioj/sim/navigation_telemetry.h"
#include "sandbox/core/vector_math.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace ioj::sim::fighters {
void apply_separation_steering(Vectors3fView const movement_directions,
                               Vectors3fConstView const separation_steering,
                               std::span<IndexSpan const> const active_spans,
                               float const separation_strength) {
    assert(movement_directions.num() == separation_steering.num());
    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= movement_directions.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const direction{make_separation_steering_direction(
                movement_directions[index], separation_steering[index], separation_strength)};
            if (direction) {
                movement_directions.set(index, *direction);
            }
        }
    }
}

void apply_navigation_choices(NavigationApplicationView const fighters,
                              std::span<IndexSpan const> const active_spans,
                              NavigationApplicationParameters const parameters,
                              NavigationScratch const& scratch,
                              NavigationTelemetrySnapshot& telemetry) {
    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto observed{static_cast<NavigationRiskTier>(scratch.observed_risk_tiers[index])};
        if (fighters.choices[element] != parameters.direct_choice) {
            observed = std::max(observed, NavigationRiskTier::Active);
        }
        auto const update{
            update_navigation_risk(static_cast<NavigationRiskTier>(fighters.risk_tiers[element]),
                                   observed,
                                   fighters.lower_risk_scan_counts[element],
                                   parameters.scans_to_demote)};
        fighters.risk_tiers[element] = static_cast<std::uint8_t>(update.tier);
        fighters.lower_risk_scan_counts[element] = update.lower_risk_scan_count;
        auto const tier_index{static_cast<std::size_t>(update.tier)};
        assert(tier_index < parameters.risk_periods.size());
        auto const period{parameters.risk_periods[tier_index]};
        fighters.periods[element] = period;
        fighters.remaining_ticks[element] = period;
    }

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= fighters.movement_directions.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            auto const steering{fighters.separation_steering[index]};
            auto const tolerance{parameters.steering_zero_tolerance};
            if (!ml::native_math::is_nearly_zero(steering.X, steering.Y, steering.Z, tolerance)) {
                ++telemetry.separating_fighter_count;
                ++telemetry.steering_memory_fighter_count;
            }
            auto const choice{fighters.choices[element]};
            if (is_avoidance_direction_choice(choice)) {
                auto const frame{make_avoidance_frame(fighters.movement_directions[index],
                                                      fighters.float_biases[element])};
                fighters.movement_directions.set(index, make_avoidance_direction(frame, choice));
                ++telemetry.avoiding_fighter_count;
            } else if (choice == parameters.stop_choice) {
                fighters.movement_directions.set(index, HMM_V3(0.f, 0.f, 0.f));
                ++telemetry.avoiding_fighter_count;
            }

            switch (static_cast<NavigationRiskTier>(fighters.risk_tiers[element])) {
                case NavigationRiskTier::Clear: {
                    ++telemetry.clear_risk_count;
                    break;
                }
                case NavigationRiskTier::Nearby: {
                    ++telemetry.nearby_risk_count;
                    break;
                }
                case NavigationRiskTier::Active: {
                    ++telemetry.active_risk_count;
                    break;
                }
                case NavigationRiskTier::Immediate: {
                    ++telemetry.immediate_risk_count;
                    break;
                }
                default: {
                    assert(false);
                    break;
                }
            }
        }
    }
}
}

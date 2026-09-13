#include "sandbox/simulation/fighter_navigation_schedule.h"

#include "sandbox/simulation/fighter_navigation.h"
#include "sandbox/simulation/fighter_navigation_scratch.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void collect_navigation_updates(std::span<FIndexSpan const> const task_spans,
                                std::span<std::int16_t> const remaining_ticks,
                                std::span<std::int16_t const> const periods,
                                NavigationScratch& scratch) {
    assert(remaining_ticks.size() == periods.size());
    auto const count{static_cast<std::int32_t>(remaining_ticks.size())};
    scratch.ready_fighter_indices.reserve(count);
    scratch.observed_risk_tiers.set_num(count);

    for (auto const span : task_spans) {
        assert(span.offset >= 0 && span.count >= 0);
        auto const end{span.end()};
        assert(end <= count);
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            if (remaining_ticks[element] > 0) {
                continue;
            }

            remaining_ticks[element] = periods[element];
            scratch.ready_fighter_indices.add(index);
            scratch.observed_risk_tiers[index] =
                static_cast<std::uint8_t>(NavigationRiskTier::Clear);
        }
    }
}
}

#include "sandbox/simulation/fighter_navigation_schedule.h"

#include "sandbox/simulation/fighter_navigation.h"
#include "sandbox/simulation/fighter_navigation_scratch.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void collect_navigation_updates(std::span<FIndexSpan const> const task_spans,
                                PeriodicTickCountdownView<std::int16_t> const countdowns,
                                NavigationScratch& scratch) {
    auto const count{static_cast<std::int32_t>(countdowns.num())};
    scratch.ready_fighter_indices.reserve(count);
    scratch.observed_risk_tiers.set_num(count);

    for (auto const span : task_spans) {
        assert(span.offset >= 0 && span.count >= 0);
        auto const end{span.end()};
        assert(end <= count);
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            if (!countdowns.try_consume(element)) {
                continue;
            }

            scratch.ready_fighter_indices.add(index);
            scratch.observed_risk_tiers[index] =
                static_cast<std::uint8_t>(NavigationRiskTier::Clear);
        }
    }
}
}

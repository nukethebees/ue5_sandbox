#include "ioj/sim/fighter_orders.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ioj::sim::fighters {
void apply_orders(OrderApplicationView const fighters,
                  FighterOrderQueueConstView const orders,
                  std::span<std::int16_t const> const navigation_periods,
                  std::int8_t const direct_choice) {
    auto const count{orders.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const order_index{static_cast<std::size_t>(index)};
        auto const found{std::ranges::find(fighters.handles, orders.handles[order_index])};
        if (found == fighters.handles.end()) {
            continue;
        }

        auto const element{static_cast<std::size_t>(found - fighters.handles.begin())};
        auto const fighter_index{static_cast<std::int32_t>(element)};
        auto const order{orders.orders[order_index]};
        if (order.task) {
            auto const old_task{fighters.tasks[element]};
            auto const new_task{orders.tasks[order_index]};
            fighters.tasks[element] = new_task;
            auto const tier{new_task == FighterTask::Standby ? NavigationRiskTier::Clear
                                                             : NavigationRiskTier::Nearby};
            auto const tier_index{static_cast<std::size_t>(tier)};
            assert(tier_index < navigation_periods.size());
            reset_navigation_state(fighters.navigation,
                                   fighter_index,
                                   tier,
                                   navigation_periods[tier_index],
                                   direct_choice);
            if (old_task != FighterTask::Attack && new_task == FighterTask::Attack) {
                fighters.desired_move_locations.set(fighter_index,
                                                    fighters.locations[fighter_index]);
                fighters.attack_reposition_countdowns[element] = 0;
            }
        }
        if (order.target) {
            fighters.targets[element] = orders.targets[order_index];
        }
    }
}
}

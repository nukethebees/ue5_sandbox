#pragma once

#include "ioj/sim/fighter_navigation_state.h"
#include "ioj/sim/fighter_order_queue.h"

namespace ioj::sim::fighters {
struct OrderApplicationView {
    std::span<RegistryEntityHandle const> handles;
    std::span<FighterTask> tasks;
    std::span<RegistryEntityHandle> targets;
    Vectors3fConstView locations;
    Vectors3fView desired_move_locations;
    std::span<std::int16_t> attack_reposition_countdowns;
    NavigationStateView navigation;
};

void apply_orders(OrderApplicationView fighters,
                  FighterOrderQueueConstView orders,
                  std::span<std::int16_t const> navigation_periods,
                  std::int8_t direct_choice);
}

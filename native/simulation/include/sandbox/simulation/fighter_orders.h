#pragma once

#include "sandbox/simulation/fighter_navigation_state.h"
#include "sandbox/simulation/fighter_order_queue.h"

namespace ml::simulation::fighters {
struct OrderApplicationView {
    std::span<FRegistryEntityHandle const> handles;
    std::span<CapitalShipFighterTask> tasks;
    std::span<FRegistryEntityHandle> targets;
    Vectors3fConstView locations;
    Vectors3fView desired_move_locations;
    std::span<std::int16_t> attack_reposition_countdowns;
    NavigationStateView navigation;
};

void apply_orders(OrderApplicationView fighters,
                  TestCapitalShipFighterOrderQueueConstView orders,
                  std::span<std::int16_t const> navigation_periods,
                  std::int8_t direct_choice);
}

#pragma once

#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/fighter_order_queue.h"
#include "sandbox/simulation/index_span.h"

#include <span>

namespace ml::simulation {
void build_capital_fighter_orders(std::span<FRegistryEntityHandle const> capital_targets,
                                  std::span<FIndexSpan const> fighter_spans,
                                  std::span<FRegistryEntityHandle const> owned_fighters,
                                  std::span<FRegistryEntityHandle const> fighter_handles,
                                  std::span<FRegistryEntityHandle const> fighter_targets,
                                  EntityRegistryQueryView registry,
                                  TestCapitalShipFighterOrderQueue& orders);
}

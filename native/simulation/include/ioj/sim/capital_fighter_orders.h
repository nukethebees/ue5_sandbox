#pragma once

#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/fighter_order_queue.h"
#include "ioj/sim/index_span.h"

#include <span>

namespace ioj::sim {
void build_fighter_orders(std::span<RegistryEntityHandle const> capital_targets,
                          std::span<IndexSpan const> fighter_spans,
                          std::span<RegistryEntityHandle const> owned_fighters,
                          std::span<RegistryEntityHandle const> fighter_handles,
                          std::span<RegistryEntityHandle const> fighter_targets,
                          EntityRegistryQueryView registry,
                          FighterOrderQueue& orders);
}

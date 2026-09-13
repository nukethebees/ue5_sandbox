#pragma once

#include "sandbox/simulation/entity_history.h"
#include "sandbox/simulation/entity_registry_bookkeeping.h"
#include "sandbox/simulation/entity_registry_statistics.h"

#include <cstdint>

namespace ml::simulation {
[[nodiscard]] auto register_spawned_entity(EntityRegistryBookkeeping& bookkeeping,
                                           EntityRegistryStatistics& statistics,
                                           EntityHistoryColumnsView history,
                                           std::int32_t slot_index,
                                           EntityUniqueId unique_id,
                                           Team team,
                                           EntityType type,
                                           std::uint8_t alive) noexcept -> FRegistryEntityHandle;
} // namespace ml::simulation

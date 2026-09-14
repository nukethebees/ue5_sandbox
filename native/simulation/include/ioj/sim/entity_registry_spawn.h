#pragma once

#include "ioj/sim/entity_history.h"
#include "ioj/sim/entity_registry_bookkeeping.h"
#include "ioj/sim/entity_registry_statistics.h"

#include <cstdint>

namespace ioj::sim {
[[nodiscard]] auto register_spawned_entity(EntityRegistryBookkeeping& bookkeeping,
                                           EntityRegistryStatistics& statistics,
                                           EntityHistoryColumnsView history,
                                           std::int32_t slot_index,
                                           EntityUniqueId unique_id,
                                           Team team,
                                           EntityType type,
                                           std::uint8_t alive) noexcept -> RegistryEntityHandle;
} // namespace ioj::sim

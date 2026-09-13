#pragma once

#include "sandbox/simulation/entity_death_info.h"
#include "sandbox/simulation/entity_registry_bookkeeping.h"
#include "sandbox/simulation/entity_registry_history.h"
#include "sandbox/simulation/entity_registry_statistics.h"

#include <cstdint>
#include <expected>

namespace ml::simulation {
struct EntityDeathAccountingError {
    UniqueIdLookupError code;
    FRegistryEntityHandle handle;
    std::int32_t event_index;
};

[[nodiscard]] auto record_entity_deaths(EntityRegistryBookkeeping& bookkeeping,
                                        EntityRegistryStatistics& statistics,
                                        EntityHistoryColumnsView history,
                                        EntityDeathInfoConstView death_events) noexcept
    -> std::expected<void, EntityDeathAccountingError>;
} // namespace ml::simulation

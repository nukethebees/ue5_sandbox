#pragma once

#include "ioj/sim/entity_death_info.h"
#include "ioj/sim/entity_registry_bookkeeping.h"
#include "ioj/sim/entity_registry_history.h"
#include "ioj/sim/entity_registry_statistics.h"

#include <cstdint>
#include <expected>

namespace ioj::sim {
struct EntityDeathAccountingError {
    UniqueIdLookupError code;
    RegistryEntityHandle handle;
    std::int32_t event_index;
};

[[nodiscard]] auto record_entity_deaths(EntityRegistryBookkeeping& bookkeeping,
                                        EntityRegistryStatistics& statistics,
                                        EntityHistoryColumnsView history,
                                        EntityDeathInfoConstView death_events) noexcept
    -> std::expected<void, EntityDeathAccountingError>;
} // namespace ioj::sim

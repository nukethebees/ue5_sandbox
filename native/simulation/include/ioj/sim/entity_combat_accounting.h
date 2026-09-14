#pragma once

#include "ioj/sim/direct_damage_events.h"
#include "ioj/sim/entity_registry_history.h"
#include "ioj/sim/entity_registry_statistics.h"

#include <cstdint>
#include <expected>
#include <span>

namespace ioj::sim {
struct EntityCombatAccountingError {
    UniqueIdLookupError code;
    RegistryEntityHandle handle;
    std::int32_t event_index;
};

[[nodiscard]] auto record_damage_events(EntityRegistryStatistics& statistics,
                                        std::span<std::int32_t const> generations,
                                        std::span<EntityUniqueId const> current_unique_ids,
                                        EntityHistoryColumnsConstView history,
                                        DirectDamageEventsConstView damage_events) noexcept
    -> std::expected<void, EntityCombatAccountingError>;

[[nodiscard]] auto record_shots(EntityRegistryStatistics& statistics,
                                std::span<std::int32_t const> generations,
                                std::span<EntityUniqueId const> current_unique_ids,
                                EntityHistoryColumnsConstView history,
                                std::span<RegistryEntityHandle const> instigators) noexcept
    -> std::expected<void, EntityCombatAccountingError>;
} // namespace ioj::sim

#pragma once

#include "sandbox/simulation/direct_damage_events.h"
#include "sandbox/simulation/entity_registry_history.h"
#include "sandbox/simulation/entity_registry_statistics.h"

#include <cstdint>
#include <expected>
#include <span>

namespace ml::simulation {
struct EntityCombatAccountingError {
    UniqueIdLookupError code;
    FRegistryEntityHandle handle;
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
                                std::span<FRegistryEntityHandle const> instigators) noexcept
    -> std::expected<void, EntityCombatAccountingError>;
} // namespace ml::simulation

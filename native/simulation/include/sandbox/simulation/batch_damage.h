#pragma once

#include "sandbox/simulation/direct_damage_events.h"
#include "sandbox/simulation/entity_death_info.h"

#include <cstdint>
#include <span>

namespace ml::simulation {
struct BatchDamageResult {
    std::int32_t removal_count;
    std::int32_t death_count;
};

[[nodiscard]] auto sort_and_deduplicate_removal_indices(std::span<std::int32_t> indices) noexcept
    -> std::int32_t;

[[nodiscard]] auto resolve_batch_damage(std::span<FRegistryEntityHandle const> entity_handles,
                                        std::span<std::int32_t> healths,
                                        DirectDamageEventsConstView damage_events,
                                        std::span<std::int32_t> removal_indices,
                                        std::int32_t removal_count,
                                        EntityDeathInfoView deaths,
                                        std::int32_t death_count) noexcept -> BatchDamageResult;
} // namespace ml::simulation

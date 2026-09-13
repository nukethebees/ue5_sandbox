#pragma once

#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ml::simulation {
[[nodiscard]] auto analyse_handle(std::span<std::int32_t const> generations,
                                  FRegistryEntityHandle handle) noexcept -> RegistryHandleState;

struct EntityRegistryBookkeeping {
    using size_type = std::int32_t;

    void reset() noexcept;
    void begin_tick() noexcept;
    void clear_queued_updates() noexcept;
    void clear_dead_entities() noexcept;

    void refresh_free_indices(std::span<std::uint8_t const> alive);
    auto available_free_slot_count() const noexcept -> size_type;
    auto take_free_slot() -> size_type;
    void append_slots(size_type count);

    void queue_update_handles(std::span<FRegistryEntityHandle const> handles);
    void record_dead(FRegistryEntityHandle handle);
    void record_moved(FRegistryEntityHandle handle);

    [[nodiscard]] auto analyse_handle(FRegistryEntityHandle handle) const noexcept
        -> RegistryHandleState;
    [[nodiscard]] auto is_valid_handle(FRegistryEntityHandle handle) const noexcept -> bool;
    [[nodiscard]] auto is_stale(FRegistryEntityHandle handle) const noexcept -> bool;

    std::vector<std::int32_t> generations;
    std::vector<EntityUniqueId> unique_ids;
    std::vector<FRegistryEntityHandle> queued_update_handles;
    std::vector<FRegistryEntityHandle> dead_entities;
    std::vector<FRegistryEntityHandle> moved_entities;
    std::vector<std::int32_t> free_indices;
};
} // namespace ml::simulation

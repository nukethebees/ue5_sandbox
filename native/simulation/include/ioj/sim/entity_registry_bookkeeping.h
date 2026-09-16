#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"
#include "ioj/sim/health.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
[[nodiscard]] auto analyse_handle(std::span<std::int32_t const> generations,
                                  RegistryEntityHandle handle) noexcept -> RegistryHandleState;

struct EntityRegistryBookkeeping {
    using size_type = std::int32_t;

    void reset() noexcept;
    void begin_tick() noexcept;
    void clear_queued_updates() noexcept;
    void clear_dead_entities() noexcept;

    void refresh_free_indices(std::span<Health const> healths);
    auto available_free_slot_count() const noexcept -> size_type;
    auto take_free_slot() -> size_type;
    void append_slots(size_type count);

    void queue_update_handles(std::span<RegistryEntityHandle const> handles);
    void record_dead(RegistryEntityHandle handle);
    void record_moved(RegistryEntityHandle handle);

    [[nodiscard]] auto analyse_handle(RegistryEntityHandle handle) const noexcept
        -> RegistryHandleState;
    [[nodiscard]] auto is_valid_handle(RegistryEntityHandle handle) const noexcept -> bool;
    [[nodiscard]] auto is_stale(RegistryEntityHandle handle) const noexcept -> bool;

    std::vector<std::int32_t> generations;
    std::vector<EntityUniqueId> unique_ids;
    std::vector<RegistryEntityHandle> queued_update_handles;
    std::vector<RegistryEntityHandle> dead_entities;
    std::vector<RegistryEntityHandle> moved_entities;
    std::vector<std::int32_t> free_indices;
};
} // namespace ioj::sim

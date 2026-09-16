#include "ioj/sim/entity_registry_bookkeeping.h"

#include <ioj/sim/health.h>

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ioj::sim {
auto analyse_handle(std::span<std::int32_t const> const generations,
                    RegistryEntityHandle const handle) noexcept -> RegistryHandleState {
    if (handle.is_null()) {
        return RegistryHandleState::Null;
    }
    if (handle.index < 0 || static_cast<std::size_t>(handle.index) >= generations.size()) {
        return RegistryHandleState::Invalid;
    }

    auto const current_generation{generations[static_cast<std::size_t>(handle.index)]};
    if (current_generation == handle.generation) {
        return RegistryHandleState::Active;
    }
    if (current_generation > handle.generation) {
        return RegistryHandleState::Stale;
    }
    return RegistryHandleState::Invalid;
}

void EntityRegistryBookkeeping::reset() noexcept {
    generations.clear();
    unique_ids.clear();
    queued_update_handles.clear();
    dead_entities.clear();
    moved_entities.clear();
    free_indices.clear();
}

void EntityRegistryBookkeeping::begin_tick() noexcept {
    moved_entities.clear();
}

void EntityRegistryBookkeeping::clear_queued_updates() noexcept {
    queued_update_handles.clear();
}

void EntityRegistryBookkeeping::clear_dead_entities() noexcept {
    dead_entities.clear();
}

void EntityRegistryBookkeeping::refresh_free_indices(std::span<Health const> const healths) {
    free_indices.clear();
    auto const count{static_cast<size_type>(healths.size())};
    for (size_type index{}; index < count; ++index) {
        if (is_dead(healths[static_cast<std::size_t>(index)])) {
            free_indices.push_back(index);
        }
    }
}

auto EntityRegistryBookkeeping::available_free_slot_count() const noexcept -> size_type {
    return static_cast<size_type>(free_indices.size());
}

auto EntityRegistryBookkeeping::take_free_slot() -> size_type {
    assert(!free_indices.empty());
    auto const slot_index{free_indices.back()};
    free_indices.pop_back();
    ++generations[static_cast<std::size_t>(slot_index)];
    return slot_index;
}

void EntityRegistryBookkeeping::append_slots(size_type const count) {
    assert(count >= 0);
    generations.resize(generations.size() + static_cast<std::size_t>(count), std::int32_t{});
    unique_ids.resize(unique_ids.size() + static_cast<std::size_t>(count));
}

void EntityRegistryBookkeeping::queue_update_handles(
    std::span<RegistryEntityHandle const> const handles) {
    queued_update_handles.insert(queued_update_handles.end(), handles.begin(), handles.end());
}

void EntityRegistryBookkeeping::record_dead(RegistryEntityHandle const handle) {
    dead_entities.push_back(handle);
}

void EntityRegistryBookkeeping::record_moved(RegistryEntityHandle const handle) {
    if (std::ranges::find(moved_entities, handle) == moved_entities.end()) {
        moved_entities.push_back(handle);
    }
}

auto EntityRegistryBookkeeping::analyse_handle(RegistryEntityHandle const handle) const noexcept
    -> RegistryHandleState {
    return sim::analyse_handle(generations, handle);
}

auto EntityRegistryBookkeeping::is_valid_handle(RegistryEntityHandle const handle) const noexcept
    -> bool {
    return analyse_handle(handle) == RegistryHandleState::Active;
}

auto EntityRegistryBookkeeping::is_stale(RegistryEntityHandle const handle) const noexcept -> bool {
    return analyse_handle(handle) == RegistryHandleState::Stale;
}
} // namespace ioj::sim

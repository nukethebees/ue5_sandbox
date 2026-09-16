#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"
#include "ioj/sim/registry_entity_handles.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ioj::sim {
struct SpawnedEntityHandles {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return registry_handles.num(); }

    [[nodiscard]] auto get_id(std::int32_t const index) const noexcept -> EntityUniqueId {
        assert(index >= 0 && index < num());
        return entity_ids[index];
    }

    void reset() noexcept {
        registry_handles.reset();
        entity_ids.clear();
    }

    [[nodiscard]] auto get_handle(std::int32_t const index) const noexcept -> RegistryEntityHandle {
        assert(index >= 0 && index < num());
        auto const storage_index{static_cast<std::size_t>(index)};
        return {registry_handles.registry_indices[storage_index],
                registry_handles.generations[storage_index]};
    }

    // Both columns retain input order, including mixed-type batches.
    RegistryEntityHandles registry_handles;
    std::vector<EntityUniqueId> entity_ids;
};
} // namespace ioj::sim

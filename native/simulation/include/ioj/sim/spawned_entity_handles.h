#pragma once

#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_types.h"
#include "ioj/sim/registry_entity_handles.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace ioj::sim {
struct SpawnedEntityHandles {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return registry_handles.num(); }

    void reset() noexcept { registry_handles.reset(); }
    void add_defaulted(std::int32_t const count) { registry_handles.add_defaulted(count); }
    void add_uninitialised(std::int32_t const count) { registry_handles.add_uninitialised(count); }

    [[nodiscard]] auto get_handle(std::int32_t const index) const noexcept -> RegistryEntityHandle {
        assert(index >= 0 && index < num());
        auto const storage_index{static_cast<std::size_t>(index)};
        return {registry_handles.registry_indices[storage_index],
                registry_handles.generations[storage_index]};
    }

    // Handles retain input order; the ID at input index i is first_id + i.
    RegistryEntityHandles registry_handles;
    EntityUniqueId first_id;
};
} // namespace ioj::sim

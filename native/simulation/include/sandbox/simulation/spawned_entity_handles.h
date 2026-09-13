#pragma once

#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_types.h"
#include "sandbox/simulation/registry_entity_handles.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace ml::simulation {
struct SpawnedEntityHandles {
    [[nodiscard]] auto num() const noexcept -> std::int32_t { return registry_handles.num(); }

    void reset() noexcept { registry_handles.reset(); }
    void add_defaulted(std::int32_t const count) { registry_handles.add_defaulted(count); }
    void add_uninitialised(std::int32_t const count) { registry_handles.add_uninitialised(count); }

    [[nodiscard]] auto get_handle(std::int32_t const index) const noexcept
        -> FRegistryEntityHandle {
        assert(index >= 0 && index < num());
        auto const storage_index{static_cast<std::size_t>(index)};
        return {registry_handles.registry_indices[storage_index],
                registry_handles.generations[storage_index]};
    }

    // Handles retain input order; the ID at input index i is first_id + i.
    ::RegistryEntityHandles registry_handles;
    EntityUniqueId first_id;
};
} // namespace ml::simulation

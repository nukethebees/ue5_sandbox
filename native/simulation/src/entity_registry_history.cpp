#include "sandbox/simulation/entity_registry_history.h"

#include "sandbox/simulation/entity_registry_bookkeeping.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation {
auto find_entity_unique_id(std::span<std::int32_t const> const generations,
                           std::span<EntityUniqueId const> const current_unique_ids,
                           EntityHistoryColumnsConstView const history,
                           FRegistryEntityHandle const handle) noexcept
    -> std::expected<EntityUniqueId, UniqueIdLookupError> {
    assert(generations.size() == current_unique_ids.size());

    switch (analyse_handle(generations, handle)) {
        case RegistryHandleState::Active:
            return current_unique_ids[static_cast<std::size_t>(handle.index)];
        case RegistryHandleState::Stale:
            break;
        case RegistryHandleState::Invalid:
        case RegistryHandleState::Null:
            return std::unexpected{UniqueIdLookupError::InvalidHandle};
    }

    auto const count{history.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (history.registry_indices[element] == handle.index &&
            history.registry_generations[element] == handle.generation) {
            return EntityUniqueId{.id = index};
        }
    }
    return std::unexpected{UniqueIdLookupError::MissingStaleHandle};
}
} // namespace ml::simulation

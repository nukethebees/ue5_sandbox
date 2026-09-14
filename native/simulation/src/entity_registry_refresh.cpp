#include "ioj/sim/entity_registry_refresh.h"

#include <cassert>
#include <cstddef>

namespace ioj::sim {
auto refresh_registry_handles(EntityRegistryQueryView const registry,
                              std::span<RegistryEntityHandle> const handles) noexcept
    -> std::int32_t {
    std::int32_t first_invalid{-1};
    auto const count{static_cast<std::int32_t>(handles.size())};
    for (std::int32_t index{}; index < count; ++index) {
        auto& handle{handles[static_cast<std::size_t>(index)]};
        switch (analyse_handle(registry, handle)) {
            case RegistryHandleState::Invalid:
                if (first_invalid < 0) {
                    first_invalid = index;
                }
                break;
            case RegistryHandleState::Null:
                break;
            case RegistryHandleState::Stale:
                handle.reset();
                break;
            case RegistryHandleState::Active:
                if (registry.alive[static_cast<std::size_t>(handle.index)] == 0) {
                    handle.reset();
                }
                break;
        }
    }
    return first_invalid;
}

} // namespace ioj::sim

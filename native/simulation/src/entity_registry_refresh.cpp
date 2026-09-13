#include "sandbox/simulation/entity_registry_refresh.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation {
namespace {
[[maybe_unused]] auto is_unused_or_sized(std::int32_t const size,
                                         std::int32_t const expected) noexcept -> bool {
    return size == 0 || size == expected;
}
}

auto refresh_registry_handles(EntityRegistryQueryView const registry,
                              std::span<FRegistryEntityHandle> const handles) noexcept
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

auto copy_registry_entity_data(EntityRegistryQueryView const registry,
                               std::span<FRegistryEntityHandle const> const handles,
                               EntityRegistryRefreshViews const outputs) noexcept -> std::int32_t {
    auto const count{static_cast<std::int32_t>(handles.size())};
    assert(is_unused_or_sized(outputs.locations.num(), count));
    assert(is_unused_or_sized(outputs.velocities.num(), count));

    std::int32_t first_inactive{-1};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const handle{handles[element]};
        if (handle.is_null()) {
            if (!outputs.locations.is_empty()) {
                outputs.locations.set(index, make_vector3f(0.0f, 0.0f, 0.0f));
            }
            if (!outputs.velocities.is_empty()) {
                outputs.velocities.set(index, make_vector3f(0.0f, 0.0f, 0.0f));
            }
            continue;
        }

        if (analyse_handle(registry, handle) != RegistryHandleState::Active) {
            if (first_inactive < 0) {
                first_inactive = index;
            }
            continue;
        }

        if (!outputs.locations.is_empty()) {
            outputs.locations.set(index, registry.locations[handle.index]);
        }
        if (!outputs.velocities.is_empty()) {
            outputs.velocities.set(index, registry.velocities[handle.index]);
        }
    }
    return first_inactive;
}
} // namespace ml::simulation

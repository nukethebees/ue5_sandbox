#include "ioj/sim/entity_registry_query.h"

#include "ioj/sim/entity_registry_bookkeeping.h"
#include "ioj/sim/health.h"

#include <algorithm>
#include <cmath>

namespace ioj::sim {
namespace {
auto byte_value(std::span<std::byte const> const values, std::int32_t const index) noexcept
    -> std::uint8_t {
    return std::to_integer<std::uint8_t>(values[static_cast<std::size_t>(index)]);
}

} // namespace

auto analyse_handle(EntityRegistryQueryView const registry,
                    RegistryEntityHandle const handle) noexcept -> RegistryHandleState {
    return analyse_handle(registry.generations, handle);
}

auto is_valid_alive(EntityRegistryQueryView const registry,
                    RegistryEntityHandle const handle) noexcept -> bool {
    return analyse_handle(registry, handle) == RegistryHandleState::Active &&
           is_alive(registry.healths[static_cast<std::size_t>(handle.index)]);
}

auto collect_entities_in_range(EntityRegistryQueryView const registry,
                               Vector3f const origin,
                               float const radius,
                               std::span<RegistryEntityHandle> const out_entities) noexcept
    -> std::int32_t {
    if (out_entities.empty()) {
        return 0;
    }

    auto const radius_squared{radius * radius};
    auto const count{registry.num()};
    std::int32_t output_count{};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const dx{registry.locations.xs[element] - origin.X};
        auto const dy{registry.locations.ys[element] - origin.Y};
        auto const dz{registry.locations.zs[element] - origin.Z};
        auto const distance_squared{dx * dx + dy * dy + dz * dz};
        if (distance_squared <= radius_squared) {
            out_entities[static_cast<std::size_t>(output_count++)] = {
                index, registry.generations[element]};
        }

        if (output_count >= static_cast<std::int32_t>(out_entities.size())) {
            break;
        }
    }
    return output_count;
}

auto collect_non_team_alive_entities(EntityRegistryQueryView const registry,
                                     Team const excluded_team,
                                     std::span<RegistryEntityHandle> const out_entities) noexcept
    -> std::int32_t {
    auto const count{registry.num()};
    std::int32_t output_count{};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (byte_value(registry.teams, index) == static_cast<std::uint8_t>(excluded_team) ||
            is_dead(registry.healths[element])) {
            continue;
        }

        if (output_count >= static_cast<std::int32_t>(out_entities.size())) {
            break;
        }
        out_entities[static_cast<std::size_t>(output_count++)] = {index,
                                                                  registry.generations[element]};
    }
    return output_count;
}

} // namespace ioj::sim

#pragma once

#include <sandbox/simulation/entity_registry_query.h>
#include <sandbox/simulation/entity_registry_update.h>

#include <sandbox/simulation/entities/TestEntityRegistry.h>

#include <cstddef>
#include <span>

namespace ml {
inline auto make_native_query_view(FTestEntityRegistry const& registry) noexcept
    -> simulation::EntityRegistryQueryView {
    static_assert(sizeof(simulation::Team) == sizeof(std::byte));
    static_assert(sizeof(simulation::EntityType) == sizeof(std::byte));

    auto const& data{registry.get_entity_data()};
    auto const count{static_cast<std::size_t>(data.num())};
    auto const teams{std::span{data.teams.data(), count}};
    auto const entity_types{std::span{data.entity_types.data(), count}};
    auto const generations{registry.get_generations()};
    return {
        .locations = data.locations.get_const_view(),
        .velocities = data.velocities.get_const_view(),
        .radii = {data.radii.data(), count},
        .generations = {generations.data(), static_cast<std::size_t>(generations.size())},
        .alive = {data.alive.data(), count},
        .teams = std::as_bytes(teams),
        .entity_types = std::as_bytes(entity_types),
    };
}

inline auto make_native_update_view(simulation::RegistryEntityData::View const view) noexcept
    -> simulation::EntityRegistryUpdateView {
    auto const count{static_cast<std::size_t>(view.num())};
    auto const teams{std::span{view.teams.data(), count}};
    auto const entity_types{std::span{view.entity_types.data(), count}};
    return {
        .locations = view.locations,
        .velocities = view.velocities,
        .rotations = view.rotations,
        .healths = {view.healths.data(), count},
        .teams = std::as_writable_bytes(teams),
        .entity_types = std::as_bytes(entity_types),
        .alive = {view.alive.data(), count},
    };
}

inline auto make_native_update_view(simulation::RegistryEntityData::ConstView const view) noexcept
    -> simulation::EntityRegistryUpdateConstView {
    auto const count{static_cast<std::size_t>(view.num())};
    auto const teams{std::span{view.teams.data(), count}};
    return {
        .locations = view.locations,
        .velocities = view.velocities,
        .rotations = view.rotations,
        .healths = {view.healths.data(), count},
        .teams = std::as_bytes(teams),
        .alive = {view.alive.data(), count},
    };
}
} // namespace ml

#pragma once

#include <sandbox/simulation/entity_registry_query.h>
#include <sandbox/simulation/entity_registry_update.h>

#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <cstddef>
#include <span>

namespace ml {
inline auto make_native_query_view(FTestEntityRegistry const& registry) noexcept
    -> simulation::EntityRegistryQueryView {
    static_assert(sizeof(ETestTeam) == sizeof(std::byte));
    static_assert(sizeof(ETestEntityType) == sizeof(std::byte));

    auto const& data{registry.get_entity_data()};
    auto const count{static_cast<std::size_t>(data.num())};
    auto const teams{std::span{data.teams.GetData(), count}};
    auto const entity_types{std::span{data.entity_types.GetData(), count}};
    auto const generations{registry.get_generations()};
    return {
        .locations = to_native(data.locations.get_const_view()),
        .velocities = to_native(data.velocities.get_const_view()),
        .radii = {data.radii.GetData(), count},
        .generations = {generations.GetData(), static_cast<std::size_t>(generations.Num())},
        .alive = {data.alive.GetData(), count},
        .teams = std::as_bytes(teams),
        .entity_types = std::as_bytes(entity_types),
    };
}

inline auto make_native_update_view(entity_registry::EntityData::View const view) noexcept
    -> simulation::EntityRegistryUpdateView {
    auto const count{static_cast<std::size_t>(view.num())};
    auto const teams{std::span{view.teams.GetData(), count}};
    auto const entity_types{std::span{view.entity_types.GetData(), count}};
    return {
        .locations = to_native(view.locations),
        .velocities = to_native(view.velocities),
        .rotations = to_native(view.rotations),
        .healths = {view.healths.GetData(), count},
        .teams = std::as_writable_bytes(teams),
        .entity_types = std::as_bytes(entity_types),
        .alive = {view.alive.GetData(), count},
    };
}

inline auto make_native_update_view(entity_registry::EntityData::ConstView const view) noexcept
    -> simulation::EntityRegistryUpdateConstView {
    auto const count{static_cast<std::size_t>(view.num())};
    auto const teams{std::span{view.teams.GetData(), count}};
    return {
        .locations = to_native(view.locations),
        .velocities = to_native(view.velocities),
        .rotations = to_native(view.rotations),
        .healths = {view.healths.GetData(), count},
        .teams = std::as_bytes(teams),
        .alive = {view.alive.GetData(), count},
    };
}
} // namespace ml

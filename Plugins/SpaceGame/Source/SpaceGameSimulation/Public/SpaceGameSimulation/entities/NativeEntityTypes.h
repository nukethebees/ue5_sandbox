#pragma once

#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/laser_source.h>

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/entities/TestTeam.h>

#include <type_traits>

namespace ml {
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::White) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::White));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Red) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::Red));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Green) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::Green));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Blue) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::Blue));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Orange) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::Orange));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Yellow) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::Yellow));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::COUNT) ==
              static_cast<std::underlying_type_t<simulation::Team>>(simulation::Team::COUNT));

static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::PlayerShip) ==
              static_cast<std::underlying_type_t<simulation::EntityType>>(
                  simulation::EntityType::PlayerShip));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::Turret) ==
    static_cast<std::underlying_type_t<simulation::EntityType>>(simulation::EntityType::Turret));
static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::CapitalShip) ==
              static_cast<std::underlying_type_t<simulation::EntityType>>(
                  simulation::EntityType::CapitalShip));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::CapitalShipFighter) ==
    static_cast<std::underlying_type_t<simulation::EntityType>>(
        simulation::EntityType::CapitalShipFighter));
static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::TubeSpinner) ==
              static_cast<std::underlying_type_t<simulation::EntityType>>(
                  simulation::EntityType::TubeSpinner));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::COUNT) ==
    static_cast<std::underlying_type_t<simulation::EntityType>>(simulation::EntityType::COUNT));

constexpr auto to_native(ETestTeam const value) noexcept -> simulation::Team {
    return static_cast<simulation::Team>(value);
}

constexpr auto to_native(ETestEntityType const value) noexcept -> simulation::EntityType {
    return static_cast<simulation::EntityType>(value);
}

constexpr auto to_unreal(simulation::Team const value) noexcept -> ETestTeam {
    return static_cast<ETestTeam>(value);
}

constexpr auto to_unreal(simulation::EntityType const value) noexcept -> ETestEntityType {
    return static_cast<ETestEntityType>(value);
}

constexpr auto make_laser_source(ETestTeam const team, ETestEntityType const type) noexcept
    -> simulation::LaserSource {
    return {.team = to_native(team), .type = to_native(type)};
}
}

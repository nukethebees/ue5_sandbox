#pragma once

#include <ioj/sim/entity_types.h>
#include <ioj/sim/laser_source.h>

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/entities/TestTeam.h>

#include <type_traits>

namespace ml {
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::White) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::White));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Red) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::Red));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Green) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::Green));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Blue) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::Blue));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Orange) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::Orange));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::Yellow) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::Yellow));
static_assert(static_cast<std::underlying_type_t<ETestTeam>>(ETestTeam::COUNT) ==
              static_cast<std::underlying_type_t<::ioj::sim::Team>>(::ioj::sim::Team::COUNT));

static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::PlayerShip) ==
              static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(
                  ::ioj::sim::EntityType::PlayerShip));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::Turret) ==
    static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(::ioj::sim::EntityType::Turret));
static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::CapitalShip) ==
              static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(
                  ::ioj::sim::EntityType::CapitalShip));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::Fighter) ==
    static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(::ioj::sim::EntityType::Fighter));
static_assert(static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::TubeSpinner) ==
              static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(
                  ::ioj::sim::EntityType::TubeSpinner));
static_assert(
    static_cast<std::underlying_type_t<ETestEntityType>>(ETestEntityType::COUNT) ==
    static_cast<std::underlying_type_t<::ioj::sim::EntityType>>(::ioj::sim::EntityType::COUNT));

constexpr auto to_native(ETestTeam const value) noexcept -> ::ioj::sim::Team {
    return static_cast<::ioj::sim::Team>(value);
}

constexpr auto to_native(ETestEntityType const value) noexcept -> ::ioj::sim::EntityType {
    return static_cast<::ioj::sim::EntityType>(value);
}

constexpr auto to_unreal(::ioj::sim::Team const value) noexcept -> ETestTeam {
    return static_cast<ETestTeam>(value);
}

constexpr auto to_unreal(::ioj::sim::EntityType const value) noexcept -> ETestEntityType {
    return static_cast<ETestEntityType>(value);
}

constexpr auto make_laser_source(ETestTeam const team, ETestEntityType const type) noexcept
    -> ::ioj::sim::LaserSource {
    return {.team = to_native(team), .type = to_native(type)};
}
}

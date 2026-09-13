#pragma once

#include <CoreMinimal.h>
#include <sandbox/simulation/ships/common/LaserFiringState.h>
#include <sandbox/simulation/ships/common/ShipLaserMode.h>
#include <sandbox/simulation/ships/common/SpaceShipCommon.h>
#include <sandbox/simulation/ships/player/TestShipFireRate.h>
#include <sandbox/simulation/ships/player/TestSpaceShipControlMode.h>
#include <sandbox/simulation/ships/player/TestSpaceShipFlightMode.h>
#include <SpaceGameSimulation/ships/common/LaserFiringState.h>
#include <SpaceGameSimulation/ships/common/ShipLaserMode.h>
#include <SpaceGameSimulation/ships/common/SpaceShipCommon.h>
#include <SpaceGameSimulation/ships/player/TestShipFireRate.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipFlightMode.h>

namespace ml {
static_assert(static_cast<std::uint8_t>(ELaserFiringState::idle) ==
              static_cast<std::uint8_t>(simulation::LaserFiringState::idle));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::burst) ==
              static_cast<std::uint8_t>(simulation::LaserFiringState::burst));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_transition) ==
              static_cast<std::uint8_t>(simulation::LaserFiringState::lock_on_transition));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_searching) ==
              static_cast<std::uint8_t>(simulation::LaserFiringState::lock_on_searching));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_acquired) ==
              static_cast<std::uint8_t>(simulation::LaserFiringState::lock_on_acquired));
inline auto to_native(ELaserFiringState const value) noexcept -> simulation::LaserFiringState {
    return static_cast<simulation::LaserFiringState>(value);
}
inline auto to_unreal(simulation::LaserFiringState const value) noexcept -> ELaserFiringState {
    return static_cast<ELaserFiringState>(value);
}

static_assert(static_cast<std::uint8_t>(EShipLaserMode::Single) ==
              static_cast<std::uint8_t>(simulation::ShipLaserMode::Single));
static_assert(static_cast<std::uint8_t>(EShipLaserMode::Double) ==
              static_cast<std::uint8_t>(simulation::ShipLaserMode::Double));
static_assert(static_cast<std::uint8_t>(EShipLaserMode::Hyper) ==
              static_cast<std::uint8_t>(simulation::ShipLaserMode::Hyper));
inline auto to_native(EShipLaserMode const value) noexcept -> simulation::ShipLaserMode {
    return static_cast<simulation::ShipLaserMode>(value);
}
inline auto to_unreal(simulation::ShipLaserMode const value) noexcept -> EShipLaserMode {
    return static_cast<EShipLaserMode>(value);
}

static_assert(static_cast<std::uint8_t>(ETestShipFireRate::Single) ==
              static_cast<std::uint8_t>(simulation::ShipFireRate::Single));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::Burst3) ==
              static_cast<std::uint8_t>(simulation::ShipFireRate::Burst3));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::FullAuto) ==
              static_cast<std::uint8_t>(simulation::ShipFireRate::FullAuto));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::COUNT) ==
              static_cast<std::uint8_t>(simulation::ShipFireRate::COUNT));
inline auto to_native(ETestShipFireRate const value) noexcept -> simulation::ShipFireRate {
    return static_cast<simulation::ShipFireRate>(value);
}
inline auto to_unreal(simulation::ShipFireRate const value) noexcept -> ETestShipFireRate {
    return static_cast<ETestShipFireRate>(value);
}

static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::Velocity) ==
              static_cast<std::uint8_t>(simulation::SpaceShipControlMode::Velocity));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::Power) ==
              static_cast<std::uint8_t>(simulation::SpaceShipControlMode::Power));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::COUNT) ==
              static_cast<std::uint8_t>(simulation::SpaceShipControlMode::COUNT));
inline auto to_native(ETestSpaceShipControlMode const value) noexcept
    -> simulation::SpaceShipControlMode {
    return static_cast<simulation::SpaceShipControlMode>(value);
}
inline auto to_unreal(simulation::SpaceShipControlMode const value) noexcept
    -> ETestSpaceShipControlMode {
    return static_cast<ETestSpaceShipControlMode>(value);
}

static_assert(static_cast<std::uint8_t>(ETestSpaceShipFlightMode::ForwardSpeed) ==
              static_cast<std::uint8_t>(simulation::SpaceShipFlightMode::ForwardSpeed));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipFlightMode::PlanarVelocity) ==
              static_cast<std::uint8_t>(simulation::SpaceShipFlightMode::PlanarVelocity));
inline auto to_native(ETestSpaceShipFlightMode const value) noexcept
    -> simulation::SpaceShipFlightMode {
    return static_cast<simulation::SpaceShipFlightMode>(value);
}
inline auto to_unreal(simulation::SpaceShipFlightMode const value) noexcept
    -> ETestSpaceShipFlightMode {
    return static_cast<ETestSpaceShipFlightMode>(value);
}

static_assert(static_cast<std::uint8_t>(EBoostBrakeState::None) ==
              static_cast<std::uint8_t>(simulation::player::BoostBrakeState::None));
static_assert(static_cast<std::uint8_t>(EBoostBrakeState::Boost) ==
              static_cast<std::uint8_t>(simulation::player::BoostBrakeState::Boost));
static_assert(static_cast<std::uint8_t>(EBoostBrakeState::Brake) ==
              static_cast<std::uint8_t>(simulation::player::BoostBrakeState::Brake));
inline auto to_native(EBoostBrakeState const value) noexcept
    -> simulation::player::BoostBrakeState {
    return static_cast<simulation::player::BoostBrakeState>(value);
}
inline auto to_unreal(simulation::player::BoostBrakeState const value) noexcept
    -> EBoostBrakeState {
    return static_cast<EBoostBrakeState>(value);
}

}

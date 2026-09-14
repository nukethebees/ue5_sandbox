#pragma once

#include <CoreMinimal.h>
#include <ioj/sim/player/control_mode.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_mode.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/space_ship_common.h>
#include <SpaceGameSimulation/ships/common/LaserFiringState.h>
#include <SpaceGameSimulation/ships/common/ShipLaserMode.h>
#include <SpaceGameSimulation/ships/common/SpaceShipCommon.h>
#include <SpaceGameSimulation/ships/player/TestShipFireRate.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipFlightMode.h>

namespace ml {
static_assert(static_cast<std::uint8_t>(ELaserFiringState::idle) ==
              static_cast<std::uint8_t>(::ioj::sim::LaserFiringState::idle));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::burst) ==
              static_cast<std::uint8_t>(::ioj::sim::LaserFiringState::burst));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_transition) ==
              static_cast<std::uint8_t>(::ioj::sim::LaserFiringState::lock_on_transition));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_searching) ==
              static_cast<std::uint8_t>(::ioj::sim::LaserFiringState::lock_on_searching));
static_assert(static_cast<std::uint8_t>(ELaserFiringState::lock_on_acquired) ==
              static_cast<std::uint8_t>(::ioj::sim::LaserFiringState::lock_on_acquired));
inline auto to_native(ELaserFiringState const value) noexcept -> ::ioj::sim::LaserFiringState {
    return static_cast<::ioj::sim::LaserFiringState>(value);
}
inline auto to_unreal(::ioj::sim::LaserFiringState const value) noexcept -> ELaserFiringState {
    return static_cast<ELaserFiringState>(value);
}

static_assert(static_cast<std::uint8_t>(EShipLaserMode::Single) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipLaserMode::Single));
static_assert(static_cast<std::uint8_t>(EShipLaserMode::Double) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipLaserMode::Double));
static_assert(static_cast<std::uint8_t>(EShipLaserMode::Hyper) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipLaserMode::Hyper));
inline auto to_native(EShipLaserMode const value) noexcept -> ::ioj::sim::ShipLaserMode {
    return static_cast<::ioj::sim::ShipLaserMode>(value);
}
inline auto to_unreal(::ioj::sim::ShipLaserMode const value) noexcept -> EShipLaserMode {
    return static_cast<EShipLaserMode>(value);
}

static_assert(static_cast<std::uint8_t>(ETestShipFireRate::Single) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipFireRate::Single));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::Burst3) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipFireRate::Burst3));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::FullAuto) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipFireRate::FullAuto));
static_assert(static_cast<std::uint8_t>(ETestShipFireRate::COUNT) ==
              static_cast<std::uint8_t>(::ioj::sim::ShipFireRate::COUNT));
inline auto to_native(ETestShipFireRate const value) noexcept -> ::ioj::sim::ShipFireRate {
    return static_cast<::ioj::sim::ShipFireRate>(value);
}
inline auto to_unreal(::ioj::sim::ShipFireRate const value) noexcept -> ETestShipFireRate {
    return static_cast<ETestShipFireRate>(value);
}

static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::Velocity) ==
              static_cast<std::uint8_t>(::ioj::sim::SpaceShipControlMode::Velocity));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::Power) ==
              static_cast<std::uint8_t>(::ioj::sim::SpaceShipControlMode::Power));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipControlMode::COUNT) ==
              static_cast<std::uint8_t>(::ioj::sim::SpaceShipControlMode::COUNT));
inline auto to_native(ETestSpaceShipControlMode const value) noexcept
    -> ::ioj::sim::SpaceShipControlMode {
    return static_cast<::ioj::sim::SpaceShipControlMode>(value);
}
inline auto to_unreal(::ioj::sim::SpaceShipControlMode const value) noexcept
    -> ETestSpaceShipControlMode {
    return static_cast<ETestSpaceShipControlMode>(value);
}

static_assert(static_cast<std::uint8_t>(ETestSpaceShipFlightMode::ForwardSpeed) ==
              static_cast<std::uint8_t>(::ioj::sim::SpaceShipFlightMode::ForwardSpeed));
static_assert(static_cast<std::uint8_t>(ETestSpaceShipFlightMode::PlanarVelocity) ==
              static_cast<std::uint8_t>(::ioj::sim::SpaceShipFlightMode::PlanarVelocity));
inline auto to_native(ETestSpaceShipFlightMode const value) noexcept
    -> ::ioj::sim::SpaceShipFlightMode {
    return static_cast<::ioj::sim::SpaceShipFlightMode>(value);
}
inline auto to_unreal(::ioj::sim::SpaceShipFlightMode const value) noexcept
    -> ETestSpaceShipFlightMode {
    return static_cast<ETestSpaceShipFlightMode>(value);
}

static_assert(static_cast<std::uint8_t>(EBoostBrakeState::None) ==
              static_cast<std::uint8_t>(::ioj::sim::player::BoostBrakeState::None));
static_assert(static_cast<std::uint8_t>(EBoostBrakeState::Boost) ==
              static_cast<std::uint8_t>(::ioj::sim::player::BoostBrakeState::Boost));
static_assert(static_cast<std::uint8_t>(EBoostBrakeState::Brake) ==
              static_cast<std::uint8_t>(::ioj::sim::player::BoostBrakeState::Brake));
inline auto to_native(EBoostBrakeState const value) noexcept
    -> ::ioj::sim::player::BoostBrakeState {
    return static_cast<::ioj::sim::player::BoostBrakeState>(value);
}
inline auto to_unreal(::ioj::sim::player::BoostBrakeState const value) noexcept
    -> EBoostBrakeState {
    return static_cast<EBoostBrakeState>(value);
}

}

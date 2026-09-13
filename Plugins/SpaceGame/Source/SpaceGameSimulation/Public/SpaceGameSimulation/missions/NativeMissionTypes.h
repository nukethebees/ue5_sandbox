#pragma once

#include <sandbox/simulation/missions/mission_fail_reason.h>
#include <sandbox/simulation/missions/mission_mode.h>
#include <sandbox/simulation/missions/mission_state.h>
#include <SpaceGameSimulation/missions/TestMissionFailReason.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>

namespace ml {
static_assert(static_cast<std::uint8_t>(simulation::MissionState::NotStarted) ==
              static_cast<std::uint8_t>(ETestMissionState::NotStarted));
static_assert(static_cast<std::uint8_t>(simulation::MissionState::Running) ==
              static_cast<std::uint8_t>(ETestMissionState::Running));
static_assert(static_cast<std::uint8_t>(simulation::MissionState::Succeeded) ==
              static_cast<std::uint8_t>(ETestMissionState::Succeeded));
static_assert(static_cast<std::uint8_t>(simulation::MissionState::Failed) ==
              static_cast<std::uint8_t>(ETestMissionState::Failed));
static_assert(static_cast<std::uint8_t>(simulation::MissionState::Disabled) ==
              static_cast<std::uint8_t>(ETestMissionState::Disabled));
inline auto to_native(ETestMissionState const value) noexcept -> simulation::MissionState {
    return static_cast<simulation::MissionState>(value);
}
inline auto to_unreal(simulation::MissionState const value) noexcept -> ETestMissionState {
    return static_cast<ETestMissionState>(value);
}

static_assert(static_cast<std::uint8_t>(simulation::MissionMode::None) ==
              static_cast<std::uint8_t>(ETestMissionMode::None));
static_assert(static_cast<std::uint8_t>(simulation::MissionMode::SurviveTime) ==
              static_cast<std::uint8_t>(ETestMissionMode::SurviveTime));
static_assert(static_cast<std::uint8_t>(simulation::MissionMode::KillEnemies) ==
              static_cast<std::uint8_t>(ETestMissionMode::KillEnemies));
static_assert(static_cast<std::uint8_t>(simulation::MissionMode::KillEnemiesWithinTime) ==
              static_cast<std::uint8_t>(ETestMissionMode::KillEnemiesWithinTime));
inline auto to_native(ETestMissionMode const value) noexcept -> simulation::MissionMode {
    return static_cast<simulation::MissionMode>(value);
}
inline auto to_unreal(simulation::MissionMode const value) noexcept -> ETestMissionMode {
    return static_cast<ETestMissionMode>(value);
}

static_assert(static_cast<std::uint8_t>(simulation::MissionFailReason::None) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::None));
static_assert(static_cast<std::uint8_t>(simulation::MissionFailReason::PlayerKilled) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::PlayerKilled));
static_assert(static_cast<std::uint8_t>(simulation::MissionFailReason::TimeElapsed) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::TimeElapsed));
static_assert(static_cast<std::uint8_t>(simulation::MissionFailReason::DefenceObjectiveFailed) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::DefenceObjectiveFailed));
inline auto to_native(ETestMissionFailReason const value) noexcept
    -> simulation::MissionFailReason {
    return static_cast<simulation::MissionFailReason>(value);
}
inline auto to_unreal(simulation::MissionFailReason const value) noexcept
    -> ETestMissionFailReason {
    return static_cast<ETestMissionFailReason>(value);
}

}

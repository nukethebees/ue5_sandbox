#pragma once

#include <ioj/sim/missions/mission_fail_reason.h>
#include <ioj/sim/missions/mission_mode.h>
#include <ioj/sim/missions/mission_state.h>
#include <SpaceGameSimulation/missions/TestMissionFailReason.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>

namespace ml {
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionState::NotStarted) ==
              static_cast<std::uint8_t>(ETestMissionState::NotStarted));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionState::Running) ==
              static_cast<std::uint8_t>(ETestMissionState::Running));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionState::Succeeded) ==
              static_cast<std::uint8_t>(ETestMissionState::Succeeded));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionState::Failed) ==
              static_cast<std::uint8_t>(ETestMissionState::Failed));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionState::Disabled) ==
              static_cast<std::uint8_t>(ETestMissionState::Disabled));
inline auto to_native(ETestMissionState const value) noexcept -> ::ioj::sim::MissionState {
    return static_cast<::ioj::sim::MissionState>(value);
}
inline auto to_unreal(::ioj::sim::MissionState const value) noexcept -> ETestMissionState {
    return static_cast<ETestMissionState>(value);
}

static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionMode::None) ==
              static_cast<std::uint8_t>(ETestMissionMode::None));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionMode::SurviveTime) ==
              static_cast<std::uint8_t>(ETestMissionMode::SurviveTime));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionMode::KillEnemies) ==
              static_cast<std::uint8_t>(ETestMissionMode::KillEnemies));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionMode::KillEnemiesWithinTime) ==
              static_cast<std::uint8_t>(ETestMissionMode::KillEnemiesWithinTime));
inline auto to_native(ETestMissionMode const value) noexcept -> ::ioj::sim::MissionMode {
    return static_cast<::ioj::sim::MissionMode>(value);
}
inline auto to_unreal(::ioj::sim::MissionMode const value) noexcept -> ETestMissionMode {
    return static_cast<ETestMissionMode>(value);
}

static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionFailReason::None) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::None));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionFailReason::PlayerKilled) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::PlayerKilled));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionFailReason::TimeElapsed) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::TimeElapsed));
static_assert(static_cast<std::uint8_t>(::ioj::sim::MissionFailReason::DefenceObjectiveFailed) ==
              static_cast<std::uint8_t>(ETestMissionFailReason::DefenceObjectiveFailed));
inline auto to_native(ETestMissionFailReason const value) noexcept
    -> ::ioj::sim::MissionFailReason {
    return static_cast<::ioj::sim::MissionFailReason>(value);
}
inline auto to_unreal(::ioj::sim::MissionFailReason const value) noexcept
    -> ETestMissionFailReason {
    return static_cast<ETestMissionFailReason>(value);
}

}

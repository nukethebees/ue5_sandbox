#include "TestMissionManager.h"

#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/misc/learning/TestEntityRegistry.h>
#include <Sandbox/misc/learning/TestSpaceShip.h>

#include <SandboxCore/uobject_utils.h>

#include <Engine/World.h>

void ATestMissionManager::begin_play() {
    if (!IsValid(player_ship)) {
        UE_LOG(LogSandbox, Warning, TEXT("player_ship is not valid."));
        set_mission_state(ETestMissionState::Disabled);
        return;
    }

    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    switch (mission_mode) {
        case ETestMissionMode::None: {
            set_mission_state(ETestMissionState::Disabled);
            break;
        }
        case ETestMissionMode::SurviveTime: {
            set_mission_state(ETestMissionState::Running);
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }

    mission_elapsed_seconds = 0.f;

    mission_start_time = GetWorld()->GetTimeSeconds();
    on_ready.Broadcast(*this);
}
void ATestMissionManager::mission_tick(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::ATestMissionManager::mission_tick);

    switch (mission_state) {
        case ETestMissionState::NotStarted: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager ticking but not started."));
            break;
        }
        case ETestMissionState::Running: {
            break;
        }
        case ETestMissionState::Succeeded: {
            return;
        }
        case ETestMissionState::Failed: {
            return;
        }
        case ETestMissionState::Disabled: {
            return;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionState."));
            break;
        }
    }

    mission_elapsed_seconds = GetWorld()->GetTimeSeconds() - mission_start_time;

    switch (mission_mode) {
        case ETestMissionMode::None: {
            break;
        }
        case ETestMissionMode::SurviveTime: {
            mission_tick_survive_seconds(dt);
            break;
        }
        case ETestMissionMode::KillEnemies: {
            mission_tick_kill_enemies(dt);
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }
}

auto ATestMissionManager::is_ready() const noexcept -> bool {
    return mission_state != ETestMissionState::NotStarted;
}

void ATestMissionManager::set_mission_mode(ETestMissionMode const new_mode) {
    mission_mode = new_mode;
}
void ATestMissionManager::set_mission_state(ETestMissionState const new_state) {
    auto const old_mission_state{new_state};
    mission_state = new_state;

    switch (mission_state) {
        case ETestMissionState::NotStarted: {
            break;
        }
        case ETestMissionState::Running: {
            break;
        }
        case ETestMissionState::Succeeded: {
            handle_mission_success();
            return;
        }
        case ETestMissionState::Failed: {
            handle_mission_failure();
            return;
        }
        case ETestMissionState::Disabled: {
            return;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionState."));
            break;
        }
    }
}

void ATestMissionManager::mission_tick_survive_seconds(float const dt) {
    if (mission_elapsed_seconds >= survive_seconds) {
        set_mission_state(ETestMissionState::Succeeded);
    }
}
void ATestMissionManager::mission_tick_kill_enemies(float const dt) {}

void ATestMissionManager::handle_mission_success() {
    UE_LOG(LogSandbox, Display, TEXT("Mission succeeded!"));

    on_mission_ended.Broadcast(*this);
}
void ATestMissionManager::handle_mission_failure() {
    UE_LOG(LogSandbox, Display, TEXT("Fission mailed."));

    on_mission_ended.Broadcast(*this);
}

#include "TestMissionManager.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestSpaceShip.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/save/SpaceSaveGame.h>
#include <Sandbox/save/SpaceSaveSubsystem.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

void FTestMissionStartupData::prune_invalid_actors() {
    entities_must_survive.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
}

void ATestMissionManager::begin_play() {
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(player_ship),
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    startup_data.prune_invalid_actors();

    if (kill_target <= 0) {
        auto const player_team{player_ship->get_team()};
        kill_target = entity_registry->count_alive_not_on_team(player_team);
    }

    switch (mission_mode) {
        case ETestMissionMode::None: {
            set_mission_state(ETestMissionState::Disabled);
            break;
        }
        case ETestMissionMode::SurviveTime:
            [[fallthrough]];
        case ETestMissionMode::KillEnemiesWithinTime:
            [[fallthrough]];
        case ETestMissionMode::KillEnemies: {
            set_mission_state(ETestMissionState::Running);
            break;
        }

        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }

    mission_elapsed_seconds = 0.f;

    on_ready.Broadcast(*this);
}

void ATestMissionManager::bind_simulation_clock(
    ATestBatchOrchestrator const& orchestrator) noexcept {
    simulation_clock.bind(orchestrator);
}

void ATestMissionManager::on_proxy_handles_bound(
    TMap<AActor const*, FRegistryEntityHandle> const& proxy_handles) {
    auto const& entities_must_survive{startup_data.entities_must_survive};
    entity_handles_that_must_survive.Reset(entities_must_survive.Num());

    for (auto const& actor : entities_must_survive) {
        if (!IsValid(actor)) {
            continue;
        }

        auto const* const handle{proxy_handles.Find(actor.Get())};
        check(handle);
        check(entity_registry->is_valid_handle(*handle));
        entity_handles_that_must_survive.Add(*handle);
    }
}

void ATestMissionManager::mission_tick() {
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

    mission_elapsed_seconds = static_cast<float>(simulation_clock.get_simulation_time());
    auto const ship_alive(player_ship->is_alive());

    if (!ship_alive) {
        set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::PlayerKilled);
        return;
    }

    if (!entity_handles_that_must_survive.IsEmpty() &&
        !entities_that_must_survive_are_alive()) {
        set_mission_state(ETestMissionState::Failed,
                          ETestMissionFailReason::DefenceObjectiveFailed);
        return;
    }

    switch (mission_mode) {
        case ETestMissionMode::None: {
            break;
        }
        case ETestMissionMode::SurviveTime: {
            mission_tick_survive_seconds();
            break;
        }
        case ETestMissionMode::KillEnemies: {
            mission_tick_kill_enemies();
            break;
        }
        case ETestMissionMode::KillEnemiesWithinTime: {
            mission_tick_kill_enemies_within_time();
            break;
        }
        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }
}

void ATestMissionManager::update_player_handles() {
    player_registry_handle = player_ship->get_entity_registry_handle();
    player_id = player_ship->get_unique_id();
}

auto ATestMissionManager::is_ready() const noexcept -> bool {
    return mission_state != ETestMissionState::NotStarted;
}

void ATestMissionManager::set_mission_mode(ETestMissionMode const new_mode) {
    mission_mode = new_mode;
}
void ATestMissionManager::set_mission_state(ETestMissionState const new_state,
                                            ETestMissionFailReason const fail_reason) {
    check((new_state == ETestMissionState::Failed) ==
          (fail_reason != ETestMissionFailReason::None));

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
            handle_mission_failure(fail_reason);
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

void ATestMissionManager::mission_tick_survive_seconds() {
    if (mission_elapsed_seconds >= target_time) {
        set_mission_state(ETestMissionState::Succeeded);
    }
}
void ATestMissionManager::mission_tick_kill_enemies() {
    auto const old_kills{player_kills};
    auto const new_kills{entity_registry->get_kills(player_id)};

    player_kills = new_kills;

    if (new_kills != old_kills) {
        on_mission_update.Broadcast(*this);
    }

    if (player_kills >= kill_target) {
        set_mission_state(ETestMissionState::Succeeded);
    }
}
void ATestMissionManager::mission_tick_kill_enemies_within_time() {
    auto const old_kills{player_kills};
    auto const new_kills{entity_registry->get_kills(player_id)};

    player_kills = new_kills;

    if (new_kills != old_kills) {
        on_mission_update.Broadcast(*this);
    }

    if (player_kills >= kill_target) {
        set_mission_state(ETestMissionState::Succeeded);
    }

    auto const mission_time{get_mission_stopwatch()};
    auto const mission_time_limit{get_target_time()};

    if (mission_time >= mission_time_limit) {
        set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::TimeElapsed);
    }
}

auto ATestMissionManager::entities_that_must_survive_are_alive() const -> bool {
    for (auto const handle : entity_handles_that_must_survive) {
        if (!entity_registry->is_valid_alive(handle)) {
            return false;
        }
    }

    return true;
}

void ATestMissionManager::handle_mission_ended(ETestMissionFailReason const fail_reason) {
    auto* world{GetWorld()};
    auto* game_instance{world->GetGameInstance()};
    auto* save_manager{game_instance->GetSubsystem<USpaceSaveSubsystem>()};

    auto const level_name{UGameplayStatics::GetCurrentLevelName(this)};

    FScoreRecord const record{
        .date = FDateTime::Now(),
        .level_name = *level_name,
        .mission_mode = mission_mode,
        .end_state = mission_state,
        .fail_reason = fail_reason,
        .kills = player_kills,
        .time_seconds = mission_elapsed_seconds,
        .target_kills = kill_target,
        .target_completion_time = target_time,
    };

    save_manager->save_score_record(record);
}
void ATestMissionManager::handle_mission_success() {
    UE_LOG(LogSandbox, Display, TEXT("Mission succeeded!"));

    handle_mission_ended(ETestMissionFailReason::None);

    on_mission_ended.Broadcast(*this);
}
void ATestMissionManager::handle_mission_failure(ETestMissionFailReason const fail_reason) {
    UE_LOG(LogSandbox, Display, TEXT("Fission mailed."));

    handle_mission_ended(fail_reason);

    on_mission_ended.Broadcast(*this);
}

#include "SpaceGame/missions/TestMissionManager.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

void FTestMissionManager::begin_play() {
    check(entity_registry);

    initialise_entity_health_that_must_survive();
    initialise_entity_health_required_to_kill();

    switch (mission_mode) {
        case ETestMissionMode::None: {
            set_mission_state(ETestMissionState::Disabled);
            break;
        }
        case ETestMissionMode::SurviveTime: {
            if (entity_handles_that_must_survive.IsEmpty()) {
                UE_LOG(LogSandbox,
                       Error,
                       TEXT("FTestMissionManager: SurviveTime requires at least one entity that "
                            "must survive"));
                set_mission_state(ETestMissionState::Disabled);
                break;
            }

            set_mission_state(ETestMissionState::Running);
            break;
        }
        case ETestMissionMode::KillEnemiesWithinTime:
            [[fallthrough]];
        case ETestMissionMode::KillEnemies: {
            if (hero_entity_ids.IsEmpty()) {
                UE_LOG(LogSandbox,
                       Error,
                       TEXT("FTestMissionManager: Kill missions require at least one hero entity"));
                set_mission_state(ETestMissionState::Disabled);
                break;
            }

            if (resolved_kill_target <= 0) {
                auto const hero_team{entity_registry->get_team(hero_entity_handles[0])};
                resolved_kill_target = entity_registry->count_alive_not_on_team(hero_team);
            }

            set_mission_state(ETestMissionState::Running);
            break;
        }

        default: {
            UE_LOG(LogSandbox, Fatal, TEXT("FTestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }
}

void FTestMissionManager::bind_simulation_clock(FSimulationClock const& clock) noexcept {
    simulation_clock.bind(clock);
}
void FTestMissionManager::reset_runtime_state() {
    pending_result_.Reset();
    hero_entity_handles.Reset();
    hero_entity_ids.Reset();
    entity_handles_that_must_survive.Reset();
    entity_ids_that_must_survive.Reset();
    entity_types_that_must_survive.Reset();
    entity_health_that_must_survive.Reset();
    entity_handles_required_to_kill.Reset();
    entity_ids_required_to_kill.Reset();
    entity_types_required_to_kill.Reset();
    entity_health_required_to_kill.Reset();
    mission_state = ETestMissionState::NotStarted;
    mission_fail_reason = ETestMissionFailReason::None;
    mission_kills = 0;
    mission_elapsed_seconds = 0.f;
    resolved_kill_target = kill_target;
}

void FTestMissionManager::set_mission_mode(ETestMissionMode const new_mode) {
    check(mission_state == ETestMissionState::NotStarted);
    mission_mode = new_mode;
}
void FTestMissionManager::set_target_time(float const new_target_time) {
    check(mission_state == ETestMissionState::NotStarted);
    check(new_target_time > 0.f);
    target_time = new_target_time;
}
void FTestMissionManager::set_kill_target(int32 const new_kill_target) {
    check(mission_state == ETestMissionState::NotStarted);
    kill_target = new_kill_target;
    resolved_kill_target = new_kill_target;
}
void FTestMissionManager::set_save_mission_results(bool const should_save) noexcept {
    check(mission_state == ETestMissionState::NotStarted);
    save_mission_results = should_save;
}
void FTestMissionManager::set_level_identity(FName const new_level_id, FString display_name) {
    check(mission_state == ETestMissionState::NotStarted);
    level_id = new_level_id;
    level_display_name = MoveTemp(display_name);
}

void FTestMissionManager::add_hero_entity(FRegistryEntityHandle handle) {
    check(mission_state == ETestMissionState::NotStarted);
    check(entity_registry->is_valid_handle(handle));
    if (hero_entity_handles.Contains(handle)) {
        return;
    }
    auto const id{entity_registry->find_unique_id(handle)};
    hero_entity_handles.Add(handle);
    hero_entity_ids.Add(id);
}

void FTestMissionManager::add_entity_that_must_survive(FRegistryEntityHandle handle) {
    check(mission_state == ETestMissionState::NotStarted);
    check(entity_registry->is_valid_handle(handle));
    if (entity_handles_that_must_survive.Contains(handle)) {
        return;
    }
    auto const id{entity_registry->find_unique_id(handle)};
    entity_handles_that_must_survive.Add(handle);
    entity_ids_that_must_survive.Add(id);
    entity_types_that_must_survive.Add(entity_registry->get_unique_entities().entity_types[id.id]);
}

void FTestMissionManager::add_entity_required_to_kill(FRegistryEntityHandle handle) {
    check(mission_state == ETestMissionState::NotStarted);
    check(entity_registry->is_valid_handle(handle));
    if (entity_handles_required_to_kill.Contains(handle)) {
        return;
    }
    auto const id{entity_registry->find_unique_id(handle)};
    entity_handles_required_to_kill.Add(handle);
    entity_ids_required_to_kill.Add(id);
    entity_types_required_to_kill.Add(entity_registry->get_unique_entities().entity_types[id.id]);
}

void FTestMissionManager::mission_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FTestMissionManager::mission_tick);

    switch (mission_state) {
        case ETestMissionState::NotStarted: {
            UE_LOG(LogSandbox, Fatal, TEXT("FTestMissionManager ticking but not started."));
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
            UE_LOG(LogSandbox, Fatal, TEXT("FTestMissionManager: Unhandled ETestMissionState."));
            break;
        }
    }

    mission_elapsed_seconds = static_cast<float>(simulation_clock.get_simulation_time());
    update_entity_health_that_must_survive();
    update_entity_health_required_to_kill();
    if (!entity_handles_that_must_survive.IsEmpty() && !entities_that_must_survive_are_alive()) {
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
            UE_LOG(LogSandbox, Fatal, TEXT("FTestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }
}

auto FTestMissionManager::complete_mission() -> bool {
    if (mission_state != ETestMissionState::Running) {
        return false;
    }

    set_mission_state(ETestMissionState::Succeeded);
    return true;
}

auto FTestMissionManager::is_ready() const noexcept -> bool {
    return mission_state != ETestMissionState::NotStarted;
}

void FTestMissionManager::set_mission_state(ETestMissionState const new_state,
                                            ETestMissionFailReason const fail_reason) {
    check((new_state == ETestMissionState::Failed) ==
          (fail_reason != ETestMissionFailReason::None));

    mission_state = new_state;
    mission_fail_reason = fail_reason;

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
            UE_LOG(LogSandbox, Fatal, TEXT("FTestMissionManager: Unhandled ETestMissionState."));
            break;
        }
    }
}

void FTestMissionManager::mission_tick_survive_seconds() {
    if (mission_elapsed_seconds >= target_time) {
        if (entities_required_to_kill_are_dead()) {
            complete_mission();
        } else {
            set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::TimeElapsed);
        }
    }
}
void FTestMissionManager::mission_tick_kill_enemies() {
    update_mission_kills();

    if (mission_kills >= resolved_kill_target && entities_required_to_kill_are_dead()) {
        complete_mission();
    }
}
void FTestMissionManager::mission_tick_kill_enemies_within_time() {
    update_mission_kills();

    if (mission_kills >= resolved_kill_target && entities_required_to_kill_are_dead()) {
        complete_mission();
        return;
    }

    auto const mission_time{get_mission_stopwatch()};
    auto const mission_time_limit{get_target_time()};

    if (mission_time >= mission_time_limit) {
        set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::TimeElapsed);
    }
}

void FTestMissionManager::update_mission_kills() {
    check(hero_entity_handles.Num() == hero_entity_ids.Num());

    mission_kills = 0;
    for (auto const id : hero_entity_ids) {
        mission_kills += entity_registry->get_kills(id);
    }
}

void FTestMissionManager::initialise_entity_health_that_must_survive() {
    entity_health_that_must_survive.Reset(entity_handles_that_must_survive.Num());
    check(entity_ids_that_must_survive.Num() == entity_handles_that_must_survive.Num());
    check(entity_types_that_must_survive.Num() == entity_handles_that_must_survive.Num());

    for (auto const handle : entity_handles_that_must_survive) {
        auto const health{entity_registry->get_health(handle)};
        entity_health_that_must_survive.Emplace(health);
    }
}

void FTestMissionManager::update_entity_health_that_must_survive() {
    check(entity_health_that_must_survive.Num() == entity_handles_that_must_survive.Num());

    auto const n_handles{entity_handles_that_must_survive.Num()};
    for (int32 i{0}; i < n_handles; ++i) {
        auto& health{entity_health_that_must_survive[i]};
        auto const handle{entity_handles_that_must_survive[i]};
        health.health =
            entity_registry->is_valid_handle(handle) ? entity_registry->get_health(handle) : 0;
    }
}

auto FTestMissionManager::entities_that_must_survive_are_alive() const -> bool {
    for (auto const handle : entity_handles_that_must_survive) {
        if (!entity_registry->is_valid_alive(handle)) {
            return false;
        }
    }

    return true;
}

void FTestMissionManager::initialise_entity_health_required_to_kill() {
    entity_health_required_to_kill.Reset(entity_handles_required_to_kill.Num());
    check(entity_ids_required_to_kill.Num() == entity_handles_required_to_kill.Num());
    check(entity_types_required_to_kill.Num() == entity_handles_required_to_kill.Num());

    for (auto const handle : entity_handles_required_to_kill) {
        auto const health{entity_registry->get_health(handle)};
        entity_health_required_to_kill.Emplace(health);
    }
}

void FTestMissionManager::update_entity_health_required_to_kill() {
    check(entity_health_required_to_kill.Num() == entity_handles_required_to_kill.Num());

    auto const n_handles{entity_handles_required_to_kill.Num()};
    for (int32 i{0}; i < n_handles; ++i) {
        auto& health{entity_health_required_to_kill[i]};
        auto const handle{entity_handles_required_to_kill[i]};
        health.health =
            entity_registry->is_valid_handle(handle) ? entity_registry->get_health(handle) : 0;
    }
}

auto FTestMissionManager::entities_required_to_kill_are_dead() const -> bool {
    for (auto const handle : entity_handles_required_to_kill) {
        if (entity_registry->is_valid_alive(handle)) {
            return false;
        }
    }

    return true;
}

void FTestMissionManager::queue_result() {
    pending_result_.Emplace(FLevelMissionResult{
        .level_id = level_id,
        .level_display_name = level_display_name,
        .mode = mission_mode,
        .state = mission_state,
        .fail_reason = mission_fail_reason,
        .kills = mission_kills,
        .elapsed_seconds = mission_elapsed_seconds,
        .target_kills = resolved_kill_target,
        .target_time = target_time,
        .save_results = save_mission_results,
    });
}
auto FTestMissionManager::take_result() -> TOptional<FLevelMissionResult> {
    auto result{MoveTemp(pending_result_)};
    pending_result_.Reset();
    return result;
}
void FTestMissionManager::handle_mission_success() {
    queue_result();
}
void FTestMissionManager::handle_mission_failure(ETestMissionFailReason) {
    queue_result();
}

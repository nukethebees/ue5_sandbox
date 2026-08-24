#include "SpaceGame/missions/TestMissionManager.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>
#include <SpaceGame/persistence/SpaceSaveSubsystem.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

void FTestMissionStartupData::prune_invalid_actors() {
    hero_entities.RemoveAll([](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
    entities_must_survive.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
    entities_required_to_kill.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
}

void FTestMissionManager::begin_play() {
    check(entity_registry);

    startup_data.prune_invalid_actors();
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

void FTestMissionManager::bind_simulation_clock(
    ATestBatchOrchestrator const& orchestrator) noexcept {
    simulation_clock.bind(orchestrator);
}
void FTestMissionManager::set_world(UWorld& new_world) noexcept {
    world = &new_world;
}
void FTestMissionManager::replace_startup_actor(AActor const* const old_actor, AActor& new_actor) {
    if (!old_actor) {
        return;
    }

    auto replace_actor{[old_actor, &new_actor](TObjectPtr<AActor>& actor) {
        if (actor == old_actor) {
            actor = &new_actor;
        }
    }};

    for (auto& actor : startup_data.hero_entities) {
        replace_actor(actor);
    }
    for (auto& actor : startup_data.entities_must_survive) {
        replace_actor(actor);
    }
    for (auto& actor : startup_data.entities_required_to_kill) {
        replace_actor(actor);
    }
}

void FTestMissionManager::reset_runtime_state() {
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

void FTestMissionManager::add_hero_entity(AActor& actor) {
    check(mission_state == ETestMissionState::NotStarted);
    startup_data.hero_entities.Add(&actor);
}
void FTestMissionManager::add_entity_that_must_survive(AActor& actor) {
    check(mission_state == ETestMissionState::NotStarted);
    startup_data.entities_must_survive.Add(&actor);
}
void FTestMissionManager::add_entity_required_to_kill(AActor& actor) {
    check(mission_state == ETestMissionState::NotStarted);
    startup_data.entities_required_to_kill.Add(&actor);
}

void FTestMissionManager::on_proxy_entities_bound(FProxyEntityMap const& proxy_entities) {
    auto resolve_identifiers{[this, &proxy_entities](AActor const& actor) {
        if (auto const* const identifiers{proxy_entities.Find(&actor)}) {
            return *identifiers;
        }

        auto const* const entity{Cast<ITestEntity>(&actor)};
        check(entity);

        auto const handle{entity->get_entity_handle()};
        check(entity_registry->is_valid_handle(handle));
        auto const unique_id{entity_registry->find_unique_id(handle)};
        check(entity_registry->is_valid_unique_id(unique_id));
        return FRegistryEntityIdentifiers{
            .handle = handle,
            .unique_id = unique_id,
        };
    }};

    auto const& hero_entities{startup_data.hero_entities};
    hero_entity_handles.Reset(hero_entities.Num());
    hero_entity_ids.Reset(hero_entities.Num());

    for (auto const& actor : hero_entities) {
        if (!IsValid(actor)) {
            continue;
        }

        auto const identifiers{resolve_identifiers(*actor)};
        if (!hero_entity_handles.Contains(identifiers.handle)) {
            hero_entity_handles.Add(identifiers.handle);
            hero_entity_ids.Add(identifiers.unique_id);
        }
    }

    auto const& entities_must_survive{startup_data.entities_must_survive};
    entity_handles_that_must_survive.Reset(entities_must_survive.Num());
    entity_ids_that_must_survive.Reset(entities_must_survive.Num());
    entity_types_that_must_survive.Reset(entities_must_survive.Num());

    for (auto const& actor : entities_must_survive) {
        if (!IsValid(actor)) {
            continue;
        }

        auto const identifiers{resolve_identifiers(*actor)};
        if (!entity_handles_that_must_survive.Contains(identifiers.handle)) {
            entity_handles_that_must_survive.Add(identifiers.handle);
            entity_ids_that_must_survive.Add(identifiers.unique_id);
            entity_types_that_must_survive.Add(
                entity_registry->get_unique_entities().entity_types[identifiers.unique_id.id]);
        }
    }

    auto const& entities_required_to_kill{startup_data.entities_required_to_kill};
    entity_handles_required_to_kill.Reset(entities_required_to_kill.Num());
    entity_ids_required_to_kill.Reset(entities_required_to_kill.Num());
    entity_types_required_to_kill.Reset(entities_required_to_kill.Num());

    for (auto const& actor : entities_required_to_kill) {
        if (!IsValid(actor)) {
            continue;
        }

        auto const identifiers{resolve_identifiers(*actor)};
        if (!entity_handles_required_to_kill.Contains(identifiers.handle)) {
            entity_handles_required_to_kill.Add(identifiers.handle);
            entity_ids_required_to_kill.Add(identifiers.unique_id);
            entity_types_required_to_kill.Add(
                entity_registry->get_unique_entities().entity_types[identifiers.unique_id.id]);
        }
    }

    if (mission_state == ETestMissionState::NotStarted) {
        initialise_entity_health_that_must_survive();
        initialise_entity_health_required_to_kill();
    }
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
            set_mission_state(ETestMissionState::Succeeded);
        } else {
            set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::TimeElapsed);
        }
    }
}
void FTestMissionManager::mission_tick_kill_enemies() {
    update_mission_kills();

    if (mission_kills >= resolved_kill_target && entities_required_to_kill_are_dead()) {
        set_mission_state(ETestMissionState::Succeeded);
    }
}
void FTestMissionManager::mission_tick_kill_enemies_within_time() {
    update_mission_kills();

    if (mission_kills >= resolved_kill_target && entities_required_to_kill_are_dead()) {
        set_mission_state(ETestMissionState::Succeeded);
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

void FTestMissionManager::handle_mission_ended(ETestMissionFailReason const fail_reason) {
    if (!save_mission_results) {
        return;
    }

    check(IsValid(world));
    auto* const game_instance{world->GetGameInstance()};
    auto* save_manager{game_instance->GetSubsystem<USpaceSaveSubsystem>()};

    auto const level_name{UGameplayStatics::GetCurrentLevelName(world)};

    FScoreRecord const record{
        .date = FDateTime::Now(),
        .level_name = *level_name,
        .mission_mode = mission_mode,
        .end_state = mission_state,
        .fail_reason = fail_reason,
        .kills = mission_kills,
        .time_seconds = mission_elapsed_seconds,
        .target_kills = resolved_kill_target,
        .target_completion_time = target_time,
    };

    save_manager->save_score_record(record);
}
void FTestMissionManager::handle_mission_success() {
    UE_LOG(LogSandbox, Display, TEXT("Mission succeeded!"));

    handle_mission_ended(ETestMissionFailReason::None);
}
void FTestMissionManager::handle_mission_failure(ETestMissionFailReason const fail_reason) {
    UE_LOG(LogSandbox, Display, TEXT("Fission mailed."));

    handle_mission_ended(fail_reason);
}

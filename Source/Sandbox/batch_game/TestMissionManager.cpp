#include "TestMissionManager.h"

#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/logging/SandboxLogCategories.h>
#include <Sandbox/save/SpaceSaveGame.h>
#include <Sandbox/save/SpaceSaveSubsystem.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

void FTestMissionStartupData::prune_invalid_actors() {
    hero_entities.RemoveAll([](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
    entities_must_survive.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
}

void ATestMissionManager::begin_play() {
    ml::fatal_if_uobject_ptrs_invalid({
        SANDBOX_NAMED_UOBJECT_PTR(entity_registry),
    });

    startup_data.prune_invalid_actors();
    mission_fail_reason = ETestMissionFailReason::None;
    mission_kills = 0;
    initialise_entity_health_that_must_survive();

    switch (mission_mode) {
        case ETestMissionMode::None: {
            set_mission_state(ETestMissionState::Disabled);
            break;
        }
        case ETestMissionMode::SurviveTime: {
            if (entity_handles_that_must_survive.IsEmpty()) {
                UE_LOG(LogSandbox,
                       Error,
                       TEXT("ATestMissionManager: SurviveTime requires at least one entity that "
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
                       TEXT("ATestMissionManager: Kill missions require at least one hero entity"));
                set_mission_state(ETestMissionState::Disabled);
                break;
            }

            if (kill_target <= 0) {
                auto const hero_team{entity_registry->get_team(hero_entity_handles[0])};
                kill_target = entity_registry->count_alive_not_on_team(hero_team);
            }

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

void ATestMissionManager::set_mission_mode(ETestMissionMode const new_mode) {
    check(mission_state == ETestMissionState::NotStarted);
    mission_mode = new_mode;
}
void ATestMissionManager::set_target_time(float const new_target_time) {
    check(mission_state == ETestMissionState::NotStarted);
    check(new_target_time > 0.f);
    target_time = new_target_time;
}
void ATestMissionManager::set_kill_target(int32 const new_kill_target) {
    check(mission_state == ETestMissionState::NotStarted);
    kill_target = new_kill_target;
}
void ATestMissionManager::set_save_mission_results(bool const should_save) noexcept {
    check(mission_state == ETestMissionState::NotStarted);
    save_mission_results = should_save;
}

void ATestMissionManager::add_hero_entity(AActor& actor) {
    check(mission_state == ETestMissionState::NotStarted);
    startup_data.hero_entities.Add(&actor);
}
void ATestMissionManager::add_entity_that_must_survive(AActor& actor) {
    check(mission_state == ETestMissionState::NotStarted);
    startup_data.entities_must_survive.Add(&actor);
}

void ATestMissionManager::on_proxy_entities_bound(FProxyEntityMap const& proxy_entities) {
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

    if (mission_state == ETestMissionState::NotStarted) {
        initialise_entity_health_that_must_survive();
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
    update_entity_health_that_must_survive();
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
            UE_LOG(LogSandbox, Fatal, TEXT("ATestMissionManager: Unhandled ETestMissionMode."));
            break;
        }
    }
}
auto ATestMissionManager::is_ready() const noexcept -> bool {
    return mission_state != ETestMissionState::NotStarted;
}

void ATestMissionManager::set_mission_state(ETestMissionState const new_state,
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
    update_mission_kills();

    if (mission_kills >= kill_target) {
        set_mission_state(ETestMissionState::Succeeded);
    }
}
void ATestMissionManager::mission_tick_kill_enemies_within_time() {
    update_mission_kills();

    if (mission_kills >= kill_target) {
        set_mission_state(ETestMissionState::Succeeded);
        return;
    }

    auto const mission_time{get_mission_stopwatch()};
    auto const mission_time_limit{get_target_time()};

    if (mission_time >= mission_time_limit) {
        set_mission_state(ETestMissionState::Failed, ETestMissionFailReason::TimeElapsed);
    }
}

void ATestMissionManager::update_mission_kills() {
    check(hero_entity_handles.Num() == hero_entity_ids.Num());

    auto const old_kills{mission_kills};

    mission_kills = 0;
    for (auto const id : hero_entity_ids) {
        mission_kills += entity_registry->get_kills(id);
    }

    if (mission_kills != old_kills) {
        on_mission_update.Broadcast(*this);
    }
}

void ATestMissionManager::initialise_entity_health_that_must_survive() {
    entity_health_that_must_survive.Reset(entity_handles_that_must_survive.Num());
    check(entity_ids_that_must_survive.Num() == entity_handles_that_must_survive.Num());
    check(entity_types_that_must_survive.Num() == entity_handles_that_must_survive.Num());

    for (auto const handle : entity_handles_that_must_survive) {
        auto const health{entity_registry->get_health(handle)};
        entity_health_that_must_survive.Emplace(health);
    }
}

void ATestMissionManager::update_entity_health_that_must_survive() {
    check(entity_health_that_must_survive.Num() == entity_handles_that_must_survive.Num());

    auto has_changed{false};
    auto const n_handles{entity_handles_that_must_survive.Num()};
    for (int32 i{0}; i < n_handles; ++i) {
        auto& health{entity_health_that_must_survive[i]};
        auto const handle{entity_handles_that_must_survive[i]};
        auto const new_health{
            entity_registry->is_valid_handle(handle) ? entity_registry->get_health(handle) : 0};
        if (health.health != new_health) {
            health.health = new_health;
            has_changed = true;
        }
    }

    if (has_changed) {
        on_mission_update.Broadcast(*this);
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
    if (!save_mission_results) {
        return;
    }

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
        .kills = mission_kills,
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

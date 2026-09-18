#include "ioj/sim/mission_manager.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/profiling.h>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/levels/level_runtime_events.h>

namespace ioj::sim {

namespace {
auto get_level_entity_id(std::span<EntityUniqueId const> const level_entity_ids,
                         std::int32_t const entity_index) -> EntityUniqueId {
    assert(entity_index >= 0 && static_cast<std::size_t>(entity_index) < level_entity_ids.size());
    auto const id{level_entity_ids[entity_index]};
    assert(id.is_valid());
    return id;
}
}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
void MissionManager::begin_play() {
    prepare_objectives();

    switch (mission_mode) {
        case MissionMode::None: {
            set_mission_state(MissionState::Disabled);
            break;
        }
        case MissionMode::SurviveTime: {
            if (entity_ids_that_must_survive.empty()) {
                ml::log_error("MissionManager: SurviveTime requires at least one entity that "
                              "must survive");
                set_mission_state(MissionState::Disabled);
                break;
            }

            set_mission_state(MissionState::Running);
            break;
        }
        case MissionMode::KillEnemiesWithinTime:
            [[fallthrough]];
        case MissionMode::KillEnemies: {
            if (hero_entity_ids.empty()) {
                ml::log_error("MissionManager: Kill missions require at least one hero entity");
                set_mission_state(MissionState::Disabled);
                break;
            }

            if (resolved_kill_target <= 0) {
                auto const hero_row{entity_ledger.get_history_index(hero_entity_ids[0])};
                auto const hero_team{entity_ledger.get_unique_entities().teams[hero_row]};
                resolved_kill_target = entity_ledger.count_alive_not_on_team(hero_team);
            }

            set_mission_state(MissionState::Running);
            break;
        }

        default: {
            ml::fatal_error("MissionManager: Unhandled MissionMode.");
        }
    }
}

MissionManager::MissionManager(SimClock const& clock,
                               EntityLedger const& in_entity_ledger,
                               AgentAccessor const& agents)
    : entity_ledger{in_entity_ledger}
    , agents_{agents}
    , simulation_clock{clock} {}

/* **************************************** */
// Level mission setup
/* **************************************** */
void MissionManager::initialise_level_mission(
    LevelMissionInitialisationData const& data,
    std::span<EntityUniqueId const> const level_entity_ids) {
    assert(!level_initialisation_applied_);
    set_level_identity(data.level_id, data.level_title);
    set_save_mission_results(data.save_results);

    switch (data.mode) {
        case levels::LevelMissionMode::Unspecified: {
            set_mission_mode(MissionMode::None);
            break;
        }
        case levels::LevelMissionMode::SurviveTime: {
            set_mission_mode(MissionMode::SurviveTime);
            break;
        }
        case levels::LevelMissionMode::KillEnemies: {
            set_mission_mode(MissionMode::KillEnemies);
            break;
        }
        case levels::LevelMissionMode::KillEnemiesWithinTime: {
            set_mission_mode(MissionMode::KillEnemiesWithinTime);
            break;
        }
    }

    if (data.time_limit_seconds.has_value()) {
        set_target_time(data.time_limit_seconds.value());
    }

    if (data.mode == levels::LevelMissionMode::KillEnemies ||
        data.mode == levels::LevelMissionMode::KillEnemiesWithinTime) {
        set_kill_target(data.kill_count.value_or(0) +
                        kill_target_increase_before_level_initialisation_);
    }

    for (auto const entity_index : data.hero_entity_indices) {
        add_hero_entity(get_level_entity_id(level_entity_ids, entity_index));
    }
    for (auto const entity_index : data.must_survive_entity_indices) {
        add_entity_that_must_survive(get_level_entity_id(level_entity_ids, entity_index));
    }
    for (auto const entity_index : data.required_kill_entity_indices) {
        add_entity_required_to_kill(get_level_entity_id(level_entity_ids, entity_index));
    }

    level_initialisation_applied_ = true;
}

void MissionManager::bind_level_event_data(std::span<std::int32_t const> const values,
                                           std::span<EntityUniqueId const> const level_entity_ids) {
    assert(mission_state == MissionState::NotStarted);
    level_event_values_ = values;
    level_entity_ids_ = level_entity_ids;
}

void MissionManager::consume_level_events(LevelMissionEventGroupsConstView const groups) {
    auto const group_count{groups.num()};
    for (std::int32_t index{}; index < group_count; ++index) {
        auto const values{level_event_values_.subspan(groups.offsets[index], groups.counts[index])};
        switch (groups.types[index]) {
            case LevelMissionEventType::MustSurvive: {
                for (auto const entity_index : values) {
                    add_entity_that_must_survive(
                        get_level_entity_id(level_entity_ids_, entity_index));
                }
                break;
            }
            case LevelMissionEventType::RequiredKill: {
                for (auto const entity_index : values) {
                    add_entity_required_to_kill(
                        get_level_entity_id(level_entity_ids_, entity_index));
                }
                break;
            }
            case LevelMissionEventType::IncreaseKillTarget: {
                for (auto const increase : values) {
                    increase_kill_target(increase);
                    if (!level_initialisation_applied_) {
                        kill_target_increase_before_level_initialisation_ += increase;
                    }
                }
                break;
            }
            default: {
                ml::fatal_error(std::format("Unsupported level mission event type: {}",
                                            static_cast<std::int32_t>(groups.types[index])));
            }
        }
    }
    if (group_count != 0) {
        objective_event_dispatched();
    }
}

/* **************************************** */
// Mission configuration and objectives
/* **************************************** */
void MissionManager::reset_runtime_state() {
    pending_result_.reset();
    hero_entity_ids.clear();
    entity_ids_that_must_survive.clear();
    entity_types_that_must_survive.clear();
    entity_health_that_must_survive.clear();
    entity_ids_required_to_kill.clear();
    entity_types_required_to_kill.clear();
    entity_health_required_to_kill.clear();

    mission_state = MissionState::NotStarted;
    mission_fail_reason = MissionFailReason::None;
    mission_kills = 0;
    mission_elapsed_seconds = 0.f;
    resolved_kill_target = kill_target;
    pending_objective_events_ = 0;
    level_event_values_ = {};
    level_entity_ids_ = {};
    kill_target_increase_before_level_initialisation_ = 0;
    level_initialisation_applied_ = false;
}

void MissionManager::set_mission_mode(MissionMode const new_mode) {
    assert(mission_state == MissionState::NotStarted);
    mission_mode = new_mode;
}
void MissionManager::set_target_time(float const new_target_time) {
    assert(mission_state == MissionState::NotStarted);
    assert(new_target_time > 0.f);
    target_time = new_target_time;
}
void MissionManager::set_kill_target(std::int32_t const new_kill_target) {
    assert(mission_state == MissionState::NotStarted);
    kill_target = new_kill_target;
    resolved_kill_target = new_kill_target;
}
void MissionManager::set_save_mission_results(bool const should_save) noexcept {
    assert(mission_state == MissionState::NotStarted);
    save_mission_results = should_save;
}
void MissionManager::set_level_identity(std::string const new_level_id, std::string display_name) {
    assert(mission_state == MissionState::NotStarted);
    level_id = new_level_id;
    level_display_name = std::move(display_name);
}

void MissionManager::add_hero_entity(EntityUniqueId id) {
    assert(mission_state == MissionState::NotStarted);
    assert(entity_ledger.is_valid_unique_id(id));
    if (std::ranges::contains(hero_entity_ids, id)) {
        return;
    }
    hero_entity_ids.push_back(id);
}

void MissionManager::add_entity_that_must_survive(EntityUniqueId id) {
    assert(mission_state == MissionState::NotStarted || mission_state == MissionState::Running);
    assert(entity_ledger.is_valid_unique_id(id));
    if (std::ranges::contains(entity_ids_that_must_survive, id)) {
        return;
    }
    entity_ids_that_must_survive.push_back(id);
    entity_types_that_must_survive.push_back(id.entity_type());
}

void MissionManager::add_entity_required_to_kill(EntityUniqueId id) {
    assert(mission_state == MissionState::NotStarted || mission_state == MissionState::Running);
    assert(entity_ledger.is_valid_unique_id(id));
    if (std::ranges::contains(entity_ids_required_to_kill, id)) {
        return;
    }
    entity_ids_required_to_kill.push_back(id);
    entity_types_required_to_kill.push_back(id.entity_type());
}

void MissionManager::increase_kill_target(std::int32_t const increase) {
    assert(increase >= 0);
    assert(mission_state == MissionState::NotStarted || mission_state == MissionState::Running);
    kill_target += increase;
    resolved_kill_target += increase;
}

void MissionManager::set_pending_objective_events(std::int32_t const count) {
    assert(mission_state == MissionState::NotStarted);
    assert(count >= 0);
    pending_objective_events_ = count;
}

void MissionManager::objective_event_dispatched() {
    assert(pending_objective_events_ > 0);
    --pending_objective_events_;
}

/* **************************************** */
// Tick and state transitions
/* **************************************** */
void MissionManager::mission_tick() {
    SANDBOX_PROFILE_SCOPE("MissionManager::mission_tick");

    switch (mission_state) {
        case MissionState::NotStarted: {
            ml::fatal_error("MissionManager ticking but not started.");
        }
        case MissionState::Running: {
            break;
        }
        case MissionState::Succeeded: {
            return;
        }
        case MissionState::Failed: {
            return;
        }
        case MissionState::Disabled: {
            return;
        }
        default: {
            ml::fatal_error("MissionManager: Unhandled MissionState.");
        }
    }

    mission_elapsed_seconds = static_cast<float>(simulation_clock.get_simulation_time());
    update_entity_health_that_must_survive();
    update_entity_health_required_to_kill();

    if (!entity_ids_that_must_survive.empty() && !entities_that_must_survive_are_alive()) {
        set_mission_state(MissionState::Failed, MissionFailReason::DefenceObjectiveFailed);
        return;
    }

    switch (mission_mode) {
        case MissionMode::None: {
            break;
        }
        case MissionMode::SurviveTime: {
            mission_tick_survive_seconds();
            break;
        }
        case MissionMode::KillEnemies: {
            mission_tick_kill_enemies();
            break;
        }
        case MissionMode::KillEnemiesWithinTime: {
            mission_tick_kill_enemies_within_time();
            break;
        }
        default: {
            ml::fatal_error("MissionManager: Unhandled MissionMode.");
        }
    }
}

auto MissionManager::complete_mission() -> bool {
    if (mission_state != MissionState::Running || has_pending_objective_events()) {
        return false;
    }

    set_mission_state(MissionState::Succeeded);
    return true;
}

auto MissionManager::is_ready() const noexcept -> bool {
    return mission_state != MissionState::NotStarted;
}

void MissionManager::set_mission_state(MissionState const new_state,
                                       MissionFailReason const fail_reason) {
    assert((new_state == MissionState::Failed) == (fail_reason != MissionFailReason::None));

    mission_state = new_state;
    mission_fail_reason = fail_reason;

    switch (mission_state) {
        case MissionState::NotStarted: {
            break;
        }
        case MissionState::Running: {
            break;
        }
        case MissionState::Succeeded: {
            handle_mission_success();
            return;
        }
        case MissionState::Failed: {
            handle_mission_failure(fail_reason);
            return;
        }
        case MissionState::Disabled: {
            return;
        }
        default: {
            ml::fatal_error("MissionManager: Unhandled MissionState.");
        }
    }
}

void MissionManager::mission_tick_survive_seconds() {
    if (mission_elapsed_seconds >= target_time) {
        if (!has_pending_objective_events() && entities_required_to_kill_are_dead()) {
            complete_mission();
        } else {
            set_mission_state(MissionState::Failed, MissionFailReason::TimeElapsed);
        }
    }
}
void MissionManager::mission_tick_kill_enemies() {
    update_mission_kills();

    if (!has_pending_objective_events() && mission_kills >= resolved_kill_target &&
        entities_required_to_kill_are_dead()) {
        complete_mission();
    }
}
void MissionManager::mission_tick_kill_enemies_within_time() {
    update_mission_kills();

    if (!has_pending_objective_events() && mission_kills >= resolved_kill_target &&
        entities_required_to_kill_are_dead()) {
        complete_mission();
        return;
    }

    auto const mission_time{get_mission_stopwatch()};
    auto const mission_time_limit{get_target_time()};

    if (mission_time >= mission_time_limit) {
        set_mission_state(MissionState::Failed, MissionFailReason::TimeElapsed);
    }
}

void MissionManager::update_mission_kills() {
    mission_kills = 0;
    for (auto const id : hero_entity_ids) {
        mission_kills += entity_ledger.get_kills(id);
    }
}

/* **************************************** */
// Objective health tracking
/* **************************************** */
void MissionManager::prepare_objectives() {
    auto initialise_pending = [&](auto const& ids, auto& healths) {
        auto const count{ids.size()};
        for (auto index{healths.size()}; index < count; ++index) {
            auto const state{agents_.read_spatial(ids[index])};
            healths.emplace_back(state ? state->health : Health{});
        }
    };
    initialise_pending(entity_ids_that_must_survive, entity_health_that_must_survive);
    initialise_pending(entity_ids_required_to_kill, entity_health_required_to_kill);
}
void MissionManager::update_entity_health_that_must_survive() {
    assert(entity_health_that_must_survive.size() == entity_ids_that_must_survive.size());
    auto const count{entity_ids_that_must_survive.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto const state{agents_.read_spatial(entity_ids_that_must_survive[i])};
        entity_health_that_must_survive[i].health = state ? state->health : Health{};
    }
}
auto MissionManager::entities_that_must_survive_are_alive() const -> bool {
    return std::ranges::all_of(entity_ids_that_must_survive,
                               [&](auto const id) { return agents_.is_alive(id); });
}
void MissionManager::update_entity_health_required_to_kill() {
    assert(entity_health_required_to_kill.size() == entity_ids_required_to_kill.size());
    auto const count{entity_ids_required_to_kill.size()};
    for (std::size_t i{}; i < count; ++i) {
        auto const state{agents_.read_spatial(entity_ids_required_to_kill[i])};
        entity_health_required_to_kill[i].health = state ? state->health : Health{};
    }
}
auto MissionManager::entities_required_to_kill_are_dead() const -> bool {
    return std::ranges::none_of(entity_ids_required_to_kill,
                                [&](auto const id) { return agents_.is_alive(id); });
}

/* **************************************** */
// Results
/* **************************************** */
void MissionManager::queue_result() {
    pending_result_.emplace(LevelMissionResult{
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
auto MissionManager::take_result() -> std::optional<LevelMissionResult> {
    auto result{std::move(pending_result_)};
    pending_result_.reset();
    return result;
}
void MissionManager::handle_mission_success() {
    queue_result();
}
void MissionManager::handle_mission_failure(MissionFailReason) {
    queue_result();
}
} // namespace ioj::sim

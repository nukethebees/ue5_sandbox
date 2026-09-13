#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/entity_handle.h>
#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/levels/LevelMissionInitialisationData.h>
#include <sandbox/simulation/missions/mission_fail_reason.h>
#include <sandbox/simulation/missions/mission_mode.h>
#include <sandbox/simulation/missions/mission_state.h>
#include <sandbox/simulation/ship_health.h>
#include <sandbox/simulation/simulation/SimulationClock.h>

#include <string>

struct FTestEntityRegistry;

namespace ml {
struct FLevelMissionEventGroupsConstView;
}

struct FLevelMissionResult {
    std::string level_id{};
    std::string level_display_name;
    ml::simulation::MissionMode mode{ml::simulation::MissionMode::None};
    ml::simulation::MissionState state{ml::simulation::MissionState::NotStarted};
    ml::simulation::MissionFailReason fail_reason{ml::simulation::MissionFailReason::None};
    std::int32_t kills{};
    float elapsed_seconds{};
    std::int32_t target_kills{};
    float target_time{};
    bool save_results{false};
};

struct FTestMissionManager {
  public:
    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    FTestMissionManager(FSimulationClock const& clock, FTestEntityRegistry& entity_registry);
    FTestMissionManager(FTestMissionManager const&) = delete;
    FTestMissionManager(FTestMissionManager&&) = delete;
    auto operator=(FTestMissionManager const&) -> FTestMissionManager& = delete;
    auto operator=(FTestMissionManager&&) -> FTestMissionManager& = delete;

    void begin_play();
    void reset_runtime_state();
    void mission_tick();
    auto complete_mission() -> bool;

    /* **************************************** */
    // Level mission setup
    /* **************************************** */
    void initialise_level_mission(ml::FLevelMissionInitialisationData const& data,
                                  std::span<FRegistryEntityHandle const> level_entity_handles);
    void bind_level_event_data(std::span<std::int32_t const> values,
                               std::span<FRegistryEntityHandle const> level_entity_handles);
    void consume_level_events(ml::FLevelMissionEventGroupsConstView groups);

    /* **************************************** */
    // Mission configuration and objectives
    /* **************************************** */
    void set_mission_mode(ml::simulation::MissionMode new_mode);
    void set_target_time(float new_target_time);
    void set_kill_target(std::int32_t new_kill_target);
    void set_save_mission_results(bool should_save) noexcept;
    void set_level_identity(std::string level_id, std::string display_name);
    void add_hero_entity(FRegistryEntityHandle handle);
    void add_entity_that_must_survive(FRegistryEntityHandle handle);
    void add_entity_required_to_kill(FRegistryEntityHandle handle);
    void increase_kill_target(std::int32_t increase);
    void set_pending_objective_events(std::int32_t count);
    void objective_event_dispatched();

    /* **************************************** */
    // Results
    /* **************************************** */
    auto take_result() -> std::optional<FLevelMissionResult>;

    /* **************************************** */
    // Queries
    /* **************************************** */
    auto get_mission_mode() const noexcept -> ml::simulation::MissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ml::simulation::MissionState {
        return mission_state;
    }

    auto get_survive_seconds() const noexcept -> float { return target_time; }
    auto get_target_time() const noexcept -> float { return target_time; }
    auto get_time_remaining() const noexcept -> float {
        return std::max(0.f, target_time - mission_elapsed_seconds);
    }
    auto get_kill_target() const noexcept -> std::int32_t { return resolved_kill_target; }
    auto get_kills_remaining() const noexcept -> std::int32_t {
        return std::max(0, resolved_kill_target - mission_kills);
    }
    auto get_mission_kills() const noexcept -> std::int32_t { return mission_kills; }
    auto get_mission_fail_reason() const noexcept -> ml::simulation::MissionFailReason {
        return mission_fail_reason;
    }
    auto get_level_id() const noexcept -> std::string { return level_id; }
    auto get_level_display_name() const noexcept -> std::string const& {
        return level_display_name;
    }
    auto should_save_mission_results() const noexcept -> bool { return save_mission_results; }
    auto get_hero_entity_handles() const noexcept -> std::span<FRegistryEntityHandle const> {
        return hero_entity_handles;
    }
    auto get_entity_handles_that_must_survive() const noexcept
        -> std::span<FRegistryEntityHandle const> {
        return entity_handles_that_must_survive;
    }
    auto get_entity_health_that_must_survive() const noexcept
        -> std::span<ml::simulation::ShipHealth const> {
        return entity_health_that_must_survive;
    }
    auto get_entity_ids_that_must_survive() const noexcept
        -> std::span<ml::simulation::EntityUniqueId const> {
        return entity_ids_that_must_survive;
    }
    auto get_entity_types_that_must_survive() const noexcept
        -> std::span<ml::simulation::EntityType const> {
        return entity_types_that_must_survive;
    }
    auto get_entity_handles_required_to_kill() const noexcept
        -> std::span<FRegistryEntityHandle const> {
        return entity_handles_required_to_kill;
    }
    auto get_entity_health_required_to_kill() const noexcept
        -> std::span<ml::simulation::ShipHealth const> {
        return entity_health_required_to_kill;
    }
    auto get_entity_ids_required_to_kill() const noexcept
        -> std::span<ml::simulation::EntityUniqueId const> {
        return entity_ids_required_to_kill;
    }
    auto get_entity_types_required_to_kill() const noexcept
        -> std::span<ml::simulation::EntityType const> {
        return entity_types_required_to_kill;
    }

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto mission_running() const noexcept -> bool {
        return mission_state == ml::simulation::MissionState::Running;
    }
    auto is_ready() const noexcept -> bool;
    auto has_pending_objective_events() const noexcept -> bool {
        return pending_objective_events_ > 0;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry; }
    auto get_entity_registry() -> FTestEntityRegistry& { return entity_registry; }
  private:
    /* **************************************** */
    // State transitions and mission modes
    /* **************************************** */
    void set_mission_state(ml::simulation::MissionState const new_state,
                           ml::simulation::MissionFailReason const fail_reason =
                               ml::simulation::MissionFailReason::None);

    void mission_tick_survive_seconds();
    void mission_tick_kill_enemies();
    void mission_tick_kill_enemies_within_time();
    void update_mission_kills();

    /* **************************************** */
    // Objective health tracking
    /* **************************************** */
    void initialise_entity_health_that_must_survive();
    void update_entity_health_that_must_survive();
    auto entities_that_must_survive_are_alive() const -> bool;
    void initialise_entity_health_required_to_kill();
    void update_entity_health_required_to_kill();
    auto entities_required_to_kill_are_dead() const -> bool;

    /* **************************************** */
    // Completion and results
    /* **************************************** */
    void handle_mission_success();
    void handle_mission_failure(ml::simulation::MissionFailReason fail_reason);

    void queue_result();

    /* **************************************** */
    // State
    /* **************************************** */
    std::optional<FLevelMissionResult> pending_result_;
    FTestEntityRegistry& entity_registry;

    std::vector<FRegistryEntityHandle> hero_entity_handles{};
    std::vector<ml::simulation::EntityUniqueId> hero_entity_ids{};
    std::vector<FRegistryEntityHandle> entity_handles_that_must_survive{};
    std::vector<ml::simulation::EntityUniqueId> entity_ids_that_must_survive{};
    std::vector<ml::simulation::EntityType> entity_types_that_must_survive{};
    std::vector<ml::simulation::ShipHealth> entity_health_that_must_survive{};
    std::vector<FRegistryEntityHandle> entity_handles_required_to_kill{};
    std::vector<ml::simulation::EntityUniqueId> entity_ids_required_to_kill{};
    std::vector<ml::simulation::EntityType> entity_types_required_to_kill{};
    std::vector<ml::simulation::ShipHealth> entity_health_required_to_kill{};

    ml::simulation::MissionState mission_state{ml::simulation::MissionState::NotStarted};

    ml::simulation::MissionFailReason mission_fail_reason{ml::simulation::MissionFailReason::None};

    ml::simulation::MissionMode mission_mode{ml::simulation::MissionMode::None};

    float target_time{60.0f};

    std::int32_t kill_target{5};

    std::int32_t resolved_kill_target{5};

    bool save_mission_results{true};

    std::string level_id{};
    std::string level_display_name{};

    std::int32_t mission_kills{0};

    std::int32_t pending_objective_events_{};
    std::span<std::int32_t const> level_event_values_{};
    std::span<FRegistryEntityHandle const> level_entity_handles_{};
    std::int32_t kill_target_increase_before_level_initialisation_{};
    bool level_initialisation_applied_{};

    float mission_elapsed_seconds{0.0f};

    FSimulationClock const& simulation_clock;
};

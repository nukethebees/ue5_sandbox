#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/lasers/phase_interface.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/level_telemetry_manager.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/mission_manager.h>
#include <ioj/sim/overlap_handler.h>
#include <ioj/sim/player/command_interface.h>
#include <ioj/sim/player/phase_interface.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/spinners/phase_interface.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/turrets/phase_interface.h>
#include <ioj/sim/turrets/sim.h>

#include <ioj/sim/level_read_view.h>
#include <ioj/sim/sim_state.h>
#include <sandbox/core/frame_memory_resource.h>

namespace ioj::sim {

struct LevelSimTestAccess;

struct LevelSimInitData {
    FixedTickLoop clock_settings{};

    LaserSimConfig lasers;
    OverlapResponseConfig overlap_response;
    CapitalShipSimConfig capital_ships;
    FighterSimConfig fighters;
    TurretSimConfig turrets;
    SpinnerSimConfig spinners;

    TeamList participating_teams;

    std::optional<player::PlayerSpawnData> player;
    CompiledLevelEvents level_events;

    collision::EntityAABBs entity_bounds{};
    collision::WorldAABBs static_bounds{};
    collision::CellCoord grid_dimensions{400, 400, 5};
    Vector3f cell_size{ml::make_vector3f(5000.f, 5000.f, 20000.f)};

    std::size_t frame_memory_capacity_bytes{16 * 1024 * 1024};

    float fighter_fire_point_distance{};

    std::optional<LevelTelemetryRunMetadata> telemetry_metadata{};
    LevelTelemetryHistoryConfig telemetry_history{};
    GameMemory* game_memory{};
    bool fighter_diagnostics_enabled{};
};

struct LevelSim {
    using tick_type = SimClock::tick_type;
    using time_type = SimClock::time_type;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    explicit LevelSim(LevelSimInitData data);
    LevelSim(LevelSim const&) = delete;
    LevelSim(LevelSim&&) = delete;
    auto operator=(LevelSim const&) -> LevelSim& = delete;
    auto operator=(LevelSim&&) -> LevelSim& = delete;

    // Call after static collision is installed.
    // Synchronizes the initial world and telemetry before transitioning to Paused.
    void finish_initialisation();
    // Requires Paused. Preserves accumulated simulation time.
    void start();
    // Repeated pauses are valid after finish_initialisation().
    void pause();
    void set_time_scale(time_type scale);

    /* **************************************** */
    // Simulation
    /* **************************************** */
    void advance(time_type dt);
    auto get_read_view() const -> LevelReadView;

    /* **************************************** */
    // Configuration and commands
    /* **************************************** */
    void set_fighter_diagnostics_enabled(bool enabled) noexcept;
    // Replaces static collision during initialization, before finish_initialisation().
    void set_static_collision(collision::WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point) -> std::int32_t;
    // Borrowed until this LevelSim is destroyed; null when the level has no player.
    auto get_player_ship_commands() noexcept -> player::CommandInterface* {
        return player_ship_commands_.has_value() ? &player_ship_commands_.value() : nullptr;
    }

    /* **************************************** */
    // Telemetry and mission results
    /* **************************************** */
    // Records interruption without changing simulation state.
    void finalize_telemetry_run(LevelTelemetryRunEndReason reason, std::string detail = {});
    // Pauses without resetting either tick accumulator.
    void complete_telemetry_run(LevelTelemetryRunEndReason reason,
                                std::optional<Team> winning_team = {});
    // Consumes the result and marks telemetry terminal without pausing.
    auto take_mission_result() -> std::optional<LevelMissionResult>;
    auto take_finalized_telemetry_run() -> std::optional<LevelTelemetryRunRecord>;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_state() const noexcept -> OrchestratorState { return state_; }
    auto get_clock() const noexcept -> SimClock const& { return clock_; }
    auto get_time_scale() const noexcept -> time_type { return clock_.get_time_scale(); }
    auto has_future_authored_spawns() const noexcept -> bool {
        return event_manager_.has_future_spawns();
    }
    auto get_player_ship_simulation() const -> player::Sim const* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto get_lasers() const -> lasers::Sim const& { return lasers_simulation_; }
    auto get_capital_ships() const -> capital_ships::Sim const& {
        return capital_ships_simulation_;
    }
    auto get_fighters() const -> fighters::Sim const& { return fighters_simulation_; }
    auto get_turrets() const -> turrets::Sim const& { return turrets_simulation_; }
    auto get_spinners() const -> spinners::Sim const& { return spinners_simulation_; }
    auto get_entity_registry() const -> EntityRegistry const& { return entity_registry_; }
    auto get_agent_indexes() const -> AgentIndexes const& { return agent_indexes_; }
    auto get_agent_accessor() const -> AgentAccessor const& { return agent_accessor_; }
    auto get_mission_manager() const -> MissionManager const& { return mission_manager_; }
    auto get_spatial_query_manager() const -> SpatialQueryManager const& { return query_manager_; }
    auto get_level_telemetry_manager() const -> LevelTelemetryManager const& {
        return level_telemetry_manager_;
    }
    auto get_frame_memory_stats() const noexcept -> ml::FrameMemoryStats {
        return frame_memory_.get_stats();
    }
  private:
    friend struct LevelSimTestAccess;

    /* **************************************** */
    // Subsystem setup
    /* **************************************** */
    void configure_subsystems(LevelSimInitData const& data);
    void configure_player(player::PlayerSpawnData const& spawn);
    void initialise_spatial_queries(LevelSimInitData& data);
    void begin_subsystems();
    void initialise_events(CompiledLevelEvents events);
    void validate_entity_handles() const;
    void rebuild_agent_indexes();

    /* **************************************** */
    // Telemetry
    /* **************************************** */
    void initialise_telemetry();

    SimClock clock_;
    OrchestratorState state_{OrchestratorState::Uninitialised};

    std::unique_ptr<GameMemory> local_game_memory_{};
    GameMemory* game_memory_{};

    ml::FrameMemoryResource frame_memory_;
    EntityRegistry entity_registry_;
    AgentIndexes agent_indexes_{clock_};
    AgentAccessor agent_accessor_{agent_indexes_};
    SpatialQueryManager query_manager_;
    OverlapHandler overlap_handler_;
    std::vector<RegistryEntityHandle> collision_dirty_entities_;

    lasers::Sim lasers_simulation_;
    lasers::PhaseInterface lasers_phase_;

    std::optional<player::Sim> player_ship_simulation_;
    std::optional<player::PhaseInterface> player_ship_phase_;
    std::optional<player::CommandInterface> player_ship_commands_;

    fighters::Sim fighters_simulation_;
    fighters::PhaseInterface fighters_phase_;

    capital_ships::Sim capital_ships_simulation_;
    capital_ships::PhaseInterface capital_ships_phase_;

    turrets::Sim turrets_simulation_;
    turrets::PhaseInterface turrets_phase_;

    spinners::Sim spinners_simulation_;
    spinners::PhaseInterface spinners_phase_;

    MissionManager mission_manager_;
    LevelEventManager event_manager_;

    LevelTelemetryManager level_telemetry_manager_;
    std::optional<LevelTelemetryRunMetadata> telemetry_metadata_{};
    std::uint64_t frame_sequence_{};
};
} // namespace ioj::sim

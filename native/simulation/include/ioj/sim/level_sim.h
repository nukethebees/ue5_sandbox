#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

struct LevelSimInitData {
    ioj::sim::FixedTickLoop clock_settings{};

    LaserSimConfig lasers;
    OverlapResponseConfig overlap_response;
    CapitalShipSimConfig capital_ships;
    FighterSimConfig fighters;
    TurretSimConfig turrets;
    SpinnerSimConfig spinners;

    ioj::sim::TeamList participating_teams;

    std::optional<ioj::sim::player::PlayerSpawnData> player;
    ioj::sim::CompiledLevelEvents level_events;

    ioj::sim::collision::EntityAABBs entity_bounds{};
    ioj::sim::collision::WorldAABBs static_bounds{};
    ioj::sim::collision::CellCoord grid_dimensions{400, 400, 5};
    ioj::sim::Vector3f cell_size{ml::make_vector3f(5000.f, 5000.f, 20000.f)};

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
    // Requires Paused. Start/pause reset realtime sampling, preserving simulation accumulation.
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
    // Telemetry and mission results
    /* **************************************** */
    // Records interruption without changing simulation state.
    void finalize_telemetry_run(ioj::sim::LevelTelemetryRunEndReason reason,
                                std::string detail = {});
    // Pauses without resetting either tick accumulator.
    void complete_telemetry_run(ioj::sim::LevelTelemetryRunEndReason reason,
                                std::optional<ioj::sim::Team> winning_team = {});
    // Consumes the result and marks telemetry terminal without pausing.
    auto take_mission_result() -> std::optional<LevelMissionResult>;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_state() const noexcept -> OrchestratorState { return state_; }
    auto get_clock() const noexcept -> SimClock const& { return clock_; }
    auto get_time_scale() const noexcept -> time_type { return clock_.get_time_scale(); }
    auto has_future_authored_spawns() const noexcept -> bool {
        return event_manager_.has_future_spawns();
    }
    auto get_player_ship_simulation() -> ioj::sim::player::Sim* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto get_player_ship_simulation() const -> ioj::sim::player::Sim const* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto get_lasers() -> ioj::sim::lasers::Sim& { return lasers_simulation_; }
    auto get_lasers() const -> ioj::sim::lasers::Sim const& { return lasers_simulation_; }
    auto get_capital_ships() -> ioj::sim::capital_ships::Sim& { return capital_ships_simulation_; }
    auto get_capital_ships() const -> ioj::sim::capital_ships::Sim const& {
        return capital_ships_simulation_;
    }
    auto get_fighters() -> ioj::sim::fighters::Sim& { return fighters_simulation_; }
    auto get_fighters() const -> ioj::sim::fighters::Sim const& { return fighters_simulation_; }
    auto get_turrets() const -> ioj::sim::turrets::Sim const& { return turrets_simulation_; }
    auto get_spinners() const -> ioj::sim::spinners::Sim const& { return spinners_simulation_; }
    auto get_entity_registry() -> EntityRegistry& { return entity_registry_; }
    auto get_entity_registry() const -> EntityRegistry const& { return entity_registry_; }
    auto get_mission_manager() -> MissionManager& { return mission_manager_; }
    auto get_mission_manager() const -> MissionManager const& { return mission_manager_; }
    auto get_spatial_query_manager() -> ioj::sim::SpatialQueryManager& { return query_manager_; }
    auto get_spatial_query_manager() const -> ioj::sim::SpatialQueryManager const& {
        return query_manager_;
    }
    auto get_level_telemetry_manager() -> LevelTelemetryManager& {
        return level_telemetry_manager_;
    }
    auto get_level_telemetry_manager() const -> LevelTelemetryManager const& {
        return level_telemetry_manager_;
    }
    auto get_frame_memory_stats() const noexcept -> ml::FrameMemoryStats {
        return frame_memory_.get_stats();
    }

    std::function<void()> on_mission_evaluated;
    std::function<void(LevelSim&)> on_end_tick;
  private:
    /* **************************************** */
    // Subsystem setup
    /* **************************************** */
    void configure_subsystems(LevelSimInitData const& data);
    void configure_player(ioj::sim::player::PlayerSpawnData const& spawn);
    void initialise_spatial_queries(LevelSimInitData& data);
    void begin_subsystems();
    void initialise_events(CompiledLevelEvents events);

    /* **************************************** */
    // Telemetry
    /* **************************************** */
    void initialise_telemetry();
    void sample_realtime_telemetry(time_type dt);

    SimClock clock_;
    OrchestratorState state_{OrchestratorState::Uninitialised};

    std::unique_ptr<GameMemory> local_game_memory_{};
    GameMemory* game_memory_{};

    ml::FrameMemoryResource frame_memory_;
    EntityRegistry entity_registry_;
    ioj::sim::SpatialQueryManager query_manager_;
    ioj::sim::OverlapHandler overlap_handler_;

    ioj::sim::lasers::Sim lasers_simulation_;
    ioj::sim::lasers::PhaseInterface lasers_phase_;

    std::optional<ioj::sim::player::Sim> player_ship_simulation_;
    std::optional<ioj::sim::player::PhaseInterface> player_ship_phase_;

    ioj::sim::fighters::Sim fighters_simulation_;
    ioj::sim::fighters::PhaseInterface fighters_phase_;

    ioj::sim::capital_ships::Sim capital_ships_simulation_;
    ioj::sim::capital_ships::PhaseInterface capital_ships_phase_;

    ioj::sim::turrets::Sim turrets_simulation_;
    ioj::sim::turrets::PhaseInterface turrets_phase_;

    ioj::sim::spinners::Sim spinners_simulation_;
    ioj::sim::spinners::PhaseInterface spinners_phase_;

    MissionManager mission_manager_;
    ioj::sim::LevelEventManager event_manager_;

    LevelTelemetryManager level_telemetry_manager_;
    ioj::sim::FixedTickLoop telemetry_tick_loop_{};
    std::optional<LevelTelemetryRunMetadata> telemetry_metadata_{};
    std::uint64_t frame_sequence_{};
};
} // namespace ioj::sim

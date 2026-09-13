#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <sandbox/simulation/combat/lasers/TestLasersPhaseInterface.h>
#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/combat/OverlapHandler.h>
#include <sandbox/simulation/defences/spinners/TestTubeSpinnersPhaseInterface.h>
#include <sandbox/simulation/defences/spinners/TestTubeSpinnersSimulation.h>
#include <sandbox/simulation/defences/turrets/TestStaticTurretsPhaseInterface.h>
#include <sandbox/simulation/defences/turrets/TestStaticTurretsSimulation.h>
#include <sandbox/simulation/entities/TeamList.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/levels/LevelEventManager.h>
#include <sandbox/simulation/memory/GameMemory.h>
#include <sandbox/simulation/missions/TestMissionManager.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsPhaseInterface.h>
#include <sandbox/simulation/ships/capital/TestCapitalShipsSimulation.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersPhaseInterface.h>
#include <sandbox/simulation/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <sandbox/simulation/ships/player/TestSpaceShipPhaseInterface.h>
#include <sandbox/simulation/ships/player/TestSpaceShipSimulation.h>
#include <sandbox/simulation/simulation/LevelTelemetryManager.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>

#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/simulation/simulation/LevelReadView.h>
#include <sandbox/simulation/simulation/LevelSimulationState.h>

struct FLevelSimulationInitData {
    static constexpr std::int32_t player_target_spawn_index{-2};

    ml::simulation::FixedTickLoop clock_settings{};

    FLaserSimulationConfig lasers;
    FOverlapResponseConfig overlap_response;
    FCapitalSimulationConfig capital_ships;
    FFighterSimulationConfig fighters;
    FTurretSimulationConfig turrets;
    FSpinnerSimulationConfig spinners;

    ml::simulation::TeamList participating_teams;

    std::optional<ml::test_space_ship::FPlayerSpawnData> player;
    ml::test_capital_ships::SpawnData capital_spawns;
    std::vector<std::int32_t> capital_target_spawn_indices;
    ml::test_static_turrets::SpawnData turret_spawns;
    ml::FCompiledLevelEvents level_events;

    ml::simulation::Vectors3f spinner_locations;
    std::vector<float> spinner_yaws;
    std::vector<std::int32_t> spinner_fire_points;
    std::vector<ml::simulation::Transform3d> turret_transforms;

    ml::simulation::collision::EntityAABBs entity_bounds{};
    ml::simulation::collision::WorldAABBs static_bounds{};
    ml::simulation::collision::CellCoord grid_dimensions{400, 400, 5};
    ml::simulation::Vector3f cell_size{ml::make_vector3f(5000.f, 5000.f, 20000.f)};

    std::size_t frame_memory_capacity_bytes{16 * 1024 * 1024};

    float fighter_fire_point_distance{};

    std::optional<FLevelTelemetryRunMetadata> telemetry_metadata{};
    FLevelTelemetryHistoryConfig telemetry_history{};
    FGameMemory* game_memory{};
    bool fighter_diagnostics_enabled{};
};

struct FLevelSimulation {
    using tick_type = FSimulationClock::tick_type;
    using time_type = FSimulationClock::time_type;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    explicit FLevelSimulation(FLevelSimulationInitData data);
    FLevelSimulation(FLevelSimulation const&) = delete;
    FLevelSimulation(FLevelSimulation&&) = delete;
    auto operator=(FLevelSimulation const&) -> FLevelSimulation& = delete;
    auto operator=(FLevelSimulation&&) -> FLevelSimulation& = delete;

    // Call after initial targets, external mission setup, and static collision are installed.
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
    auto get_read_view() const -> FLevelReadView;

    /* **************************************** */
    // Telemetry and mission results
    /* **************************************** */
    // Records interruption without changing simulation state.
    void finalize_telemetry_run(ml::simulation::LevelTelemetryRunEndReason reason,
                                std::string detail = {});
    // Pauses without resetting either tick accumulator.
    void complete_telemetry_run(ml::simulation::LevelTelemetryRunEndReason reason,
                                std::optional<ml::simulation::Team> winning_team = {});
    // Consumes the result and marks telemetry terminal without pausing.
    auto take_mission_result() -> std::optional<FLevelMissionResult>;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_state() const noexcept -> EOrchestratorState { return state_; }
    auto get_clock() const noexcept -> FSimulationClock const& { return clock_; }
    auto get_time_scale() const noexcept -> time_type { return clock_.get_time_scale(); }
    auto has_future_authored_spawns() const noexcept -> bool {
        return event_manager_.has_future_spawns();
    }
    auto get_player_ship_simulation() -> ml::test_space_ship::Simulation* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto get_player_ship_simulation() const -> ml::test_space_ship::Simulation const* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto get_lasers() -> ml::test_lasers::Simulation& { return lasers_simulation_; }
    auto get_lasers() const -> ml::test_lasers::Simulation const& { return lasers_simulation_; }
    auto get_capital_ships() -> ml::test_capital_ships::Simulation& {
        return capital_ships_simulation_;
    }
    auto get_capital_ships() const -> ml::test_capital_ships::Simulation const& {
        return capital_ships_simulation_;
    }
    auto get_capital_ship_fighters() -> ml::test_capital_ship_fighters::Simulation& {
        return capital_ship_fighters_simulation_;
    }
    auto get_capital_ship_fighters() const -> ml::test_capital_ship_fighters::Simulation const& {
        return capital_ship_fighters_simulation_;
    }
    auto get_turrets() -> ml::test_static_turrets::Simulation& { return turrets_simulation_; }
    auto get_turrets() const -> ml::test_static_turrets::Simulation const& {
        return turrets_simulation_;
    }
    auto get_spinners() -> ml::test_tube_spinners::Simulation& { return spinners_simulation_; }
    auto get_spinners() const -> ml::test_tube_spinners::Simulation const& {
        return spinners_simulation_;
    }
    auto get_entity_registry() -> FTestEntityRegistry& { return entity_registry_; }
    auto get_entity_registry() const -> FTestEntityRegistry const& { return entity_registry_; }
    auto get_mission_manager() -> FTestMissionManager& { return mission_manager_; }
    auto get_mission_manager() const -> FTestMissionManager const& { return mission_manager_; }
    auto get_spatial_query_manager() -> ml::FSpatialQueryManager& { return query_manager_; }
    auto get_spatial_query_manager() const -> ml::FSpatialQueryManager const& {
        return query_manager_;
    }
    auto get_level_telemetry_manager() -> FLevelTelemetryManager& {
        return level_telemetry_manager_;
    }
    auto get_level_telemetry_manager() const -> FLevelTelemetryManager const& {
        return level_telemetry_manager_;
    }
    auto get_frame_memory_stats() const noexcept -> ml::FrameMemoryStats {
        return frame_memory_.get_stats();
    }

    std::function<void()> on_mission_evaluated;
    std::function<void(FLevelSimulation&)> on_end_tick;
  private:
    /* **************************************** */
    // Subsystem setup
    /* **************************************** */
    void configure_subsystems(FLevelSimulationInitData const& data);
    void configure_player(ml::test_space_ship::FPlayerSpawnData const& spawn);
    void initialise_spatial_queries(FLevelSimulationInitData& data);
    void begin_subsystems(FLevelSimulationInitData const& data);
    void initialise_events(FLevelSimulationInitData& data);

    /* **************************************** */
    // Telemetry
    /* **************************************** */
    void initialise_telemetry();
    void sample_realtime_telemetry(time_type dt);

    FSimulationClock clock_;
    EOrchestratorState state_{EOrchestratorState::Uninitialised};

    std::unique_ptr<FGameMemory> local_game_memory_{};
    FGameMemory* game_memory_{};

    ml::FrameMemoryResource frame_memory_;
    FTestEntityRegistry entity_registry_;
    ml::FSpatialQueryManager query_manager_;
    ml::FOverlapHandler overlap_handler_;

    ml::test_lasers::Simulation lasers_simulation_;
    ml::test_lasers::PhaseInterface lasers_phase_;

    std::optional<ml::test_space_ship::Simulation> player_ship_simulation_;
    std::optional<ml::test_space_ship::PhaseInterface> player_ship_phase_;

    ml::test_capital_ship_fighters::Simulation capital_ship_fighters_simulation_;
    ml::test_capital_ship_fighters::PhaseInterface capital_ship_fighters_phase_;

    ml::test_capital_ships::Simulation capital_ships_simulation_;
    ml::test_capital_ships::PhaseInterface capital_ships_phase_;

    ml::test_static_turrets::Simulation turrets_simulation_;
    ml::test_static_turrets::PhaseInterface turrets_phase_;

    ml::test_tube_spinners::Simulation spinners_simulation_;
    ml::test_tube_spinners::PhaseInterface spinners_phase_;

    FTestMissionManager mission_manager_;
    ml::FLevelEventManager event_manager_;

    FLevelTelemetryManager level_telemetry_manager_;
    ml::simulation::FixedTickLoop telemetry_tick_loop_{};
    std::optional<FLevelTelemetryRunMetadata> telemetry_metadata_{};
    std::uint64_t frame_sequence_{};
};

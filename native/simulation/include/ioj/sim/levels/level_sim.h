#pragma once
#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/collision/collision_pipeline.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/levels/level_read_access.h>
#include <ioj/sim/levels/level_sim_init_data.h>
#include <ioj/sim/levels/run_stop_conditions.h>
#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/player/command_interface.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/sim_state.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/telemetry/manager.h>
#include <ioj/sim/turrets/sim.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {

struct LevelSimTestAccess;

struct LevelSim {
    using tick_type = SimClock::tick_type;
    using time_type = SimClock::time_type;

    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    LevelSim(GameMemory& game_memory, LevelSimInitData data);

    LevelSim(LevelSim const&) = delete;
    LevelSim(LevelSim&&) = delete;

    auto operator=(LevelSim const&) -> LevelSim& = delete;
    auto operator=(LevelSim&&) -> LevelSim& = delete;

    void finish_initialisation();
    void start();
    void pause();

    /* **************************************** */
    // Simulation
    /* **************************************** */
    void advance(time_type dt);
    auto get_read_access() const -> LevelReadAccess { return LevelReadAccess{*this}; }

    /* **************************************** */
    // Configuration and commands
    /* **************************************** */
    void set_telemetry_metadata(LevelTelemetryRunMetadata metadata);
    void set_static_collision(std::span<collision::WorldAABB const> bounds);
    auto get_player_ship_commands() noexcept -> std::optional<player::CommandInterface> {
        if (!player_ship_simulation_) {
            return std::nullopt;
        }

        return std::optional<player::CommandInterface>{std::in_place, *player_ship_simulation_};
    }

    /* **************************************** */
    // Telemetry and mission results
    /* **************************************** */
    void finalize_telemetry_run(LevelTelemetryRunEndReason reason);
    void stop_run(LevelTelemetryRunEndReason reason);
    void stop_run_if_requested(RunStopConditions const& conditions);
    auto take_player_team_completion() -> std::optional<TeamCompletion>;
    auto take_finalized_telemetry_run() -> std::optional<LevelTelemetryRunRecord>;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto get_state() const noexcept -> OrchestratorState { return state_; }
    auto get_clock() const noexcept -> SimClock const& { return clock_; }
    auto get_time_scale() const noexcept -> PositiveFinite<time_type> {
        return clock_.get_time_scale();
    }
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
    auto get_entity_ledger() const -> EntityLedger const& { return entity_ledger_; }
    auto get_entity_tables() const -> EntityTables const& { return entity_tables_; }
    auto get_mission() const -> Mission const& { return mission_; }
    auto get_result_metadata() const -> LevelResultMetadata const& { return result_metadata_; }
    auto get_level_id() const -> std::string const& { return result_metadata_.level_id; }
    auto get_level_title() const -> std::string const& { return result_metadata_.level_title; }
    auto get_spatial_query_manager() const -> SpatialQueryManager const& {
        return collision_pipeline_.queries();
    }
    auto get_level_telemetry_manager() const -> LevelTelemetryManager const& {
        return level_telemetry_manager_;
    }
    auto get_frame_memory_stats() const noexcept -> FrameMemoryStats {
        return frame_memory_.get_stats();
    }
  private:
    void refresh_spatial_index();
    friend struct LevelSimTestAccess;
    friend class LevelReadAccess;

    /* **************************************** */
    // Subsystem setup
    /* **************************************** */
    void initialise_subsystems(LevelSimInitData const& data);

    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void prepare_tick(float tick_period, bool player_active);
    void think_tick(float tick_period, bool player_active);
    void act_tick(float tick_period, bool player_active);
    void resolve_tick(bool player_active);

    SimClock clock_;
    OrchestratorState state_{OrchestratorState::Uninitialised};

    memory::Block frame_memory_block_;
    FrameMemoryResource frame_memory_;
    EntityLedger entity_ledger_;
    CombatEvents combat_events_{entity_ledger_};
    EntityTables entity_tables_{clock_};
    collision::CollisionPipeline collision_pipeline_;

    lasers::Sim lasers_simulation_;
    std::optional<player::Sim> player_ship_simulation_;
    fighters::Sim fighters_simulation_;
    capital_ships::Sim capital_ships_simulation_;
    turrets::Sim turrets_simulation_;
    spinners::Sim spinners_simulation_;

    Mission mission_;
    LevelResultMetadata result_metadata_;
    LevelEventManager event_manager_;
    LevelTelemetryManager level_telemetry_manager_;
};
} // namespace ioj::sim

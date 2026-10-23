#pragma once
#include <ioj/column_math.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/frame_vectors3f.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_tables.h>
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/fighters/sim.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/levels/level_event_manager.h>
#include <ioj/sim/levels/level_read_access.h>
#include <ioj/sim/levels/level_sim_init_data.h>
#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/missions/mission_manager.h>
#include <ioj/sim/overlap_handler.h>
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

    // Call after static collision is installed.
    void finish_initialisation();
    void start();
    void pause();
    void set_time_scale(PositiveFinite<time_type> scale);

    /* **************************************** */
    // Simulation
    /* **************************************** */
    void advance(time_type dt);
    auto get_read_access() const -> LevelReadAccess { return LevelReadAccess{*this}; }

    /* **************************************** */
    // Configuration and commands
    /* **************************************** */
    void set_fighter_diagnostics_enabled(bool enabled) noexcept;
    void set_telemetry_metadata(LevelTelemetryRunMetadata metadata);
    void set_static_collision(collision::WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point)
        -> collision::StaticGeometryIndex;
    auto get_player_ship_commands() noexcept -> std::optional<player::CommandInterface> {
        if (!player_ship_simulation_) {
            return std::nullopt;
        }

        return std::optional<player::CommandInterface>{std::in_place, *player_ship_simulation_};
    }

    /* **************************************** */
    // Telemetry and mission results
    /* **************************************** */
    // Finalizes telemetry without changing simulation state.
    void finalize_telemetry_run(LevelTelemetryRunEndReason reason);
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
    auto get_mission_manager() const -> MissionManager const& { return mission_manager_; }
    auto get_spatial_query_manager() const -> SpatialQueryManager const& { return query_manager_; }
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
    void configure_subsystems(LevelSimInitData const& data);
    void configure_player(player::PlayerSpawnData const& spawn);
    void initialise_spatial_queries(LevelSimInitData& data);
    void begin_subsystems();
    void initialise_events(CompiledLevelEvents events);

    SimClock clock_;
    OrchestratorState state_{OrchestratorState::Uninitialised};

    memory::Block frame_memory_block_;
    FrameMemoryResource frame_memory_;
    EntityLedger entity_ledger_;
    CombatEvents combat_events_{entity_ledger_};
    EntityTables entity_tables_{clock_};
    SpatialQueryManager query_manager_;
    OverlapHandler overlap_handler_;
    std::vector<EntityId> overlap_candidates_;

    lasers::Sim lasers_simulation_;
    std::optional<player::Sim> player_ship_simulation_;
    fighters::Sim fighters_simulation_;
    capital_ships::Sim capital_ships_simulation_;
    turrets::Sim turrets_simulation_;
    spinners::Sim spinners_simulation_;

    MissionManager mission_manager_;
    LevelEventManager event_manager_;
    LevelTelemetryManager level_telemetry_manager_;
};
} // namespace ioj::sim

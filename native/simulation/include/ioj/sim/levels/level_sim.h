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
#include <ioj/sim/levels/level_memory_manager.h>
#include <ioj/sim/levels/level_read_access.h>
#include <ioj/sim/levels/level_sim_init_data.h>
#include <ioj/sim/levels/level_spawn_manager.h>
#include <ioj/sim/levels/level_state.h>
#include <ioj/sim/levels/tick_player_ship_state.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/player/command_interface.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/spinners/sim.h>
#include <ioj/sim/tasks/task_manager.h>
#include <ioj/sim/telemetry/level_telemetry_manager.h>
#include <ioj/sim/turrets/sim.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <memory_resource>
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
    explicit LevelSim(LevelSimInitData data,
                      std::pmr::memory_resource& memory = *std::pmr::get_default_resource());

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
    // Return the last executed tick's counters; leave the output unchanged if no tick runs.
    void advance(time_type dt, fighters::NavigationCounters* navigation_output = nullptr);
    auto read_access() const -> LevelReadAccess { return LevelReadAccess{*this}; }

    /* **************************************** */
    // Configuration and commands
    /* **************************************** */
    void set_telemetry_metadata(telemetry::RunMetadata metadata);
    void set_static_collision(std::span<collision::WorldAABB const> bounds);
    auto player_ship_commands() noexcept -> std::optional<player::CommandInterface> {
        if (!player_ship_simulation_) {
            return std::nullopt;
        }

        return std::optional<player::CommandInterface>{std::in_place, *player_ship_simulation_};
    }

    /* **************************************** */
    // Telemetry and mission results
    /* **************************************** */
    void finalize_telemetry_run(telemetry::RunEndReason reason);
    void stop_run(telemetry::RunEndReason reason);
    auto player_team_completion() const -> std::optional<TeamMissionCompletion>;
    auto take_finalized_telemetry_run() -> std::optional<telemetry::RunRecord>;

    /* **************************************** */
    // Accessors
    /* **************************************** */
    auto state() const noexcept -> LevelState { return state_; }
    auto clock() const noexcept -> SimClock const& { return clock_; }
    auto player_ship_simulation() const -> player::Sim const* {
        return player_ship_simulation_.has_value() ? &player_ship_simulation_.value() : nullptr;
    }
    auto lasers() const -> lasers::Sim const& { return lasers_simulation_; }
    auto capital_ships() const -> capital_ships::Sim const& { return capital_ships_simulation_; }
    auto fighters() const -> fighters::Sim const& { return fighters_simulation_; }
    auto turrets() const -> turrets::Sim const& { return turrets_simulation_; }
    auto spinners() const -> spinners::Sim const& { return spinners_simulation_; }
    auto entity_ledger() const -> EntityLedger const& { return entity_ledger_; }
    auto entity_tables() const -> EntityTables const& { return entity_tables_; }
    auto mission() const -> Mission const& { return mission_; }
    auto expressions() const -> ExprManager const& { return expressions_; }
    auto events() const -> LevelEventManager const& { return event_manager_; }
    auto task_manager() const -> tasks::TaskManager const& { return task_manager_; }
    auto level_metadata() const -> levels::LevelMetadata const& { return level_metadata_; }
    auto spatial_query_manager() const -> SpatialQueryManager const& {
        return collision_pipeline_.queries();
    }
    auto level_telemetry_manager() const -> LevelTelemetryManager const& {
        return level_telemetry_manager_;
    }
    auto memory_stats() const noexcept -> memory::Statistics {
        return memory_manager_.game_memory().stats();
    }
    auto tick_memory_stats() const noexcept -> FrameMemoryStats {
        return memory_manager_.tick_memory().stats();
    }
    auto phase_memory_stats() const noexcept -> FrameMemoryStats {
        return memory_manager_.phase_memory().stats();
    }
  private:
    void refresh_spatial_index();
    void dispatch_tick_end_events();
    void dispatch_preparation_events();
    friend struct LevelSimTestAccess;
    friend class LevelReadAccess;

    /* **************************************** */
    // Subsystem setup
    /* **************************************** */
    void initialise_subsystems(LevelSimInitData const& data);

    /* **************************************** */
    // Tick phases
    /* **************************************** */
    template <TickPlayerShipState PlayerState>
    void advance_tick(float tick_period, fighters::NavigationCounters* navigation_output);
    template <TickPlayerShipState PlayerState>
    void prepare_tick(float tick_period);
    template <TickPlayerShipState PlayerState>
    void think_tick(float tick_period,
                    FrameArray<std::uint32_t>& fighter_fire_indices,
                    FrameArray<std::uint32_t>& spinner_fire_indices,
                    fighters::NavigationCounters* navigation_output);
    template <TickPlayerShipState PlayerState>
    void act_tick(float tick_period,
                  std::span<std::uint32_t const> fighter_fire_indices,
                  std::span<std::uint32_t const> spinner_fire_indices,
                  std::pmr::vector<lasers::LaserIndex>& laser_removal_indices);
    template <TickPlayerShipState PlayerState>
    void resolve_tick(std::pmr::vector<lasers::LaserIndex>& laser_removal_indices);

    LevelState state_{LevelState::Uninitialised};
    SimClock clock_;
    TeamList participating_teams_;
    std::int32_t fighter_per_team_limit_{};

    LevelMemoryManager memory_manager_;
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

    tasks::TaskManager task_manager_;
    Mission mission_;
    ExprManager expressions_;
    Timers timers_;
    levels::LevelMetadata level_metadata_;
    LevelEventManager event_manager_;
    LevelSpawnManager spawn_manager_;
    LevelTelemetryManager level_telemetry_manager_;
};
} // namespace ioj::sim

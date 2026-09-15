#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/level_sim.h>
#include <ioj/sim/profiling.h>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/monotonic_clock.h>
#include <span>
#include <thread>
#include <vector>

namespace ioj::sim {

namespace level_simulation {
class TickTimingCollector {
  public:
    explicit TickTimingCollector(bool const enabled)
        : enabled_{enabled}
        , tick_started_at_{enabled ? ml::monotonic_seconds() : 0.0}
        , phase_started_at_{tick_started_at_} {
        system_timings_.fill(-1.0);
        phase_timings_.fill(-1.0);
        for (auto& systems : phase_system_timings_) {
            systems.fill(-1.0);
        }
    }

    class Scope {
      public:
        Scope(TickTimingCollector& collector, SimTelemetryTimingSystem const system)
            : collector_{collector}
            , system_index_{static_cast<std::size_t>(system)}
            , phase_index_{static_cast<std::size_t>(collector.phase_)}
            , started_at_{collector.enabled_ ? ml::monotonic_seconds() : 0.0} {}
        Scope(Scope const&) = delete;
        Scope(Scope&&) = delete;
        auto operator=(Scope const&) -> Scope& = delete;
        auto operator=(Scope&&) -> Scope& = delete;
        ~Scope() {
            if (collector_.enabled_) {
                auto const elapsed{ml::monotonic_seconds() - started_at_};
                auto& total{collector_.system_timings_[system_index_]};
                auto& phase_total{collector_.phase_system_timings_[phase_index_][system_index_]};
                total = std::max(0.0, total) + elapsed;
                phase_total = std::max(0.0, phase_total) + elapsed;
            }
        }
      private:
        TickTimingCollector& collector_;
        std::size_t system_index_{};
        std::size_t phase_index_{};
        double started_at_{};
    };

    [[nodiscard]] auto scope(SimTelemetryTimingSystem const system) -> Scope {
        return Scope{*this, system};
    }

    void finish_phase() {
        if (enabled_) {
            auto const now{ml::monotonic_seconds()};
            phase_timings_[static_cast<std::size_t>(phase_)] = now - phase_started_at_;
            phase_started_at_ = now;
        }
    }
    void set_phase(LevelTelemetryTimingPhase const phase) { phase_ = phase; }
    void record(LevelTelemetryManager& manager) const {
        if (enabled_) {
            manager.record_simulation_tick_timing(ml::monotonic_seconds() - tick_started_at_,
                                                  system_timings_,
                                                  phase_timings_,
                                                  phase_system_timings_);
        }
    }
  private:
    bool enabled_{};
    double tick_started_at_{};
    double phase_started_at_{};
    LevelTelemetryTimingPhase phase_{LevelTelemetryTimingPhase::Preparation};
    SimTelemetryPerformanceWindow::SystemTimings system_timings_;
    SimTelemetryPerformanceWindow::PhaseArray<double> phase_timings_;
    SimTelemetryPerformanceWindow::PhaseSystemTimings phase_system_timings_{};
};

/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(LevelSimInitData& data) {
    std::array<std::uint8_t, static_cast<std::size_t>(Team::COUNT)> included{};
    auto const included_count{included.size()};
    auto include = [&included](Team const team) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < included_count) {
            included[team_index] = 1;
        } else {
            ml::log_error(std::format("Ignoring invalid participating team {}", team_index));
        }
    };

    for (auto const team : data.participating_teams) {
        include(team);
    }
    if (data.participating_teams.is_empty()) {
        if (data.player.has_value()) {
            include(data.player->team);
        }
        for (auto const team : data.level_events.initial_spawns.capital_spawns.teams) {
            include(team);
        }
        for (auto const team : data.level_events.initial_spawns.turret_spawns.teams) {
            include(team);
        }
        for (auto const team : data.level_events.schedule.capital_spawns.teams) {
            include(team);
        }
        for (auto const team : data.level_events.schedule.turret_spawns.teams) {
            include(team);
        }
    }

    data.participating_teams.reset();
    for (std::int32_t team_index{}; static_cast<std::size_t>(team_index) < included_count;
         ++team_index) {
        if (included[team_index] != 0) {
            data.participating_teams.add(static_cast<Team>(team_index));
        }
    }
}

}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(LevelSimInitData data)
    : local_game_memory_{data.game_memory == nullptr ? std::make_unique<GameMemory>() : nullptr}
    , game_memory_{data.game_memory != nullptr ? data.game_memory : local_game_memory_.get()}
    , frame_memory_{data.frame_memory_capacity_bytes}
    , query_manager_{entity_registry_}
    , overlap_handler_{entity_registry_, data.overlap_response}
    , lasers_simulation_{clock_, entity_registry_, query_manager_, frame_memory_}
    , lasers_phase_{lasers_simulation_}
    , fighters_simulation_{clock_,
                           entity_registry_,
                           query_manager_,
                           lasers_simulation_,
                           frame_memory_}
    , fighters_phase_{fighters_simulation_}
    , capital_ships_simulation_{entity_registry_,
                                query_manager_,
                                fighters_simulation_,
                                frame_memory_}
    , capital_ships_phase_{capital_ships_simulation_}
    , turrets_simulation_{clock_,
                          entity_registry_,
                          query_manager_,
                          lasers_simulation_,
                          frame_memory_}
    , turrets_phase_{turrets_simulation_}
    , spinners_simulation_{clock_, entity_registry_, lasers_simulation_, frame_memory_}
    , spinners_phase_{spinners_simulation_}
    , mission_manager_{clock_, entity_registry_}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     mission_manager_}
    , level_telemetry_manager_{clock_,
                               entity_registry_,
                               lasers_simulation_,
                               query_manager_,
                               *game_memory_,
                               data.telemetry_history} {
    clock_.initialise(data.clock_settings);
    telemetry_metadata_ = std::move(data.telemetry_metadata);
    level_simulation::finalise_participating_teams(data);

    initialise_spatial_queries(data);
    configure_subsystems(data);
    begin_subsystems();

    initialise_events(std::move(data.level_events));
    event_manager_.execute_tick(0);
}
void LevelSim::finish_initialisation() {
    assert(state_ == OrchestratorState::Uninitialised);

    entity_registry_.commit_updates();
    query_manager_.update(clock_.get_completed_ticks());
    entity_registry_.end_tick();

    // Initialization rebuilds must not contribute to runtime telemetry.
    query_manager_.reset_runtime_telemetry();
    initialise_telemetry();

    event_manager_.configure_mission();
    mission_manager_.begin_play();

    state_ = OrchestratorState::Paused;
}
void LevelSim::start() {
    assert(state_ == OrchestratorState::Paused);
    telemetry_tick_loop_.initialise();
    state_ = OrchestratorState::Running;
}
void LevelSim::pause() {
    assert(state_ != OrchestratorState::Uninitialised);
    telemetry_tick_loop_.initialise();
    state_ = OrchestratorState::Paused;
}
void LevelSim::set_time_scale(time_type scale) {
    assert(scale > 0.0);
    clock_.tick_loop.time_scale = scale;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void LevelSim::configure_subsystems(LevelSimInitData const& data) {
    set_fighter_diagnostics_enabled(data.fighter_diagnostics_enabled);
    if (data.player.has_value()) {
        configure_player(data.player.value());
    }

    lasers_simulation_.set_config(data.lasers);
    capital_ships_simulation_.set_config(data.capital_ships);
    fighters_simulation_.set_config(
        data.fighters,
        {.participating_teams = data.participating_teams,
         .collision_radius = query_manager_.get_entity_type_radius(EntityType::Fighter),
         .fire_point_distance = data.fighter_fire_point_distance});
    turrets_simulation_.set_config(data.turrets);
    spinners_simulation_.set_config(data.spinners);
}
void LevelSim::configure_player(player::PlayerSpawnData const& spawn) {
    auto& player{player_ship_simulation_.emplace(
        clock_, entity_registry_, query_manager_, lasers_simulation_)};
    player_ship_phase_.emplace(player);
    player_ship_commands_.emplace(player);

    player.configure(spawn);
}
void LevelSim::initialise_spatial_queries(LevelSimInitData& data) {
    query_manager_.initialise(data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency())));
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(
        std::move(data.static_bounds));
}
void LevelSim::begin_subsystems() {
    if (player_ship_simulation_.has_value()) {
        player_ship_phase_->begin_play();
    }

    capital_ships_phase_.begin_play();
    fighters_phase_.begin_play();
    turrets_phase_.begin_play();

    spinners_phase_.begin_play();
    lasers_phase_.begin_play();
}
void LevelSim::initialise_events(CompiledLevelEvents events) {
    auto const player_handle{player_ship_simulation_.has_value()
                                 ? player_ship_simulation_->registry_handle
                                 : RegistryEntityHandle{}};
    event_manager_.initialise(std::move(events), player_handle);
}

/* **************************************** */
// Configuration and commands
/* **************************************** */
void LevelSim::set_fighter_diagnostics_enabled(bool const enabled) noexcept {
    capital_ships_simulation_.set_diagnostics_enabled(enabled);
    fighters_simulation_.set_diagnostics_enabled(enabled);
}
void LevelSim::set_static_collision(collision::WorldAABBs bounds) {
    assert(state_ == OrchestratorState::Uninitialised);
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(std::move(bounds));
}
auto LevelSim::add_static_collision_aabb(Vector3f const min_point, Vector3f const max_point)
    -> std::int32_t {
    return query_manager_.get_collision_system().get_uniform_grid().add_static_aabb(min_point,
                                                                                    max_point);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void LevelSim::initialise_telemetry() {
    level_telemetry_manager_.initialise();
    telemetry_tick_loop_.tick_rate = 4.0;
    telemetry_tick_loop_.time_scale = 1.0;
    telemetry_tick_loop_.initialise();

    if (telemetry_metadata_.has_value()) {
        level_telemetry_manager_.begin_run(std::move(telemetry_metadata_.value()));
        telemetry_metadata_.reset();
    }
}
void LevelSim::finalize_telemetry_run(LevelTelemetryRunEndReason const reason, std::string detail) {
    level_telemetry_manager_.finalize_interrupted(reason, std::move(detail));
}
void LevelSim::complete_telemetry_run(LevelTelemetryRunEndReason const reason,
                                      std::optional<Team> winning_team) {
    level_telemetry_manager_.finalize_completed(reason, winning_team);
    state_ = OrchestratorState::Paused;
}
auto LevelSim::take_mission_result() -> std::optional<LevelMissionResult> {
    auto result{mission_manager_.take_result()};
    if (!result.has_value()) {
        return std::nullopt;
    }

    level_telemetry_manager_.mark_mission_terminal(*result);
    return result;
}
auto LevelSim::take_finalized_telemetry_run() -> std::optional<LevelTelemetryRunRecord> {
    return level_telemetry_manager_.take_finalized_run();
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance");

    if (state_ != OrchestratorState::Running) {
        return;
    }

    ++frame_sequence_;
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    query_manager_.get_collision_system().reset_frame_events();
    level_telemetry_manager_.observe_frame(dt);
    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_loop.tick_period)};
        level_simulation::TickTimingCollector timings{
            level_telemetry_manager_.detailed_timing_enabled() &&
            (clock_.completed_ticks % 16) == 0};
        auto const player_active{player_ship_simulation_.has_value() &&
                                 player_ship_simulation_->health.is_alive()};
        auto publish_entity_state = [&] {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::publish_entity_state");
            if (player_ship_simulation_.has_value() && player_ship_simulation_->health.is_alive()) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->update_entity_registry();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.update_entity_registry();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.update_entity_registry();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.update_entity_registry();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Registry)};
                entity_registry_.commit_updates();
            }
        };

        /* -------------------------------------------------------------------------------- */
        // Preparation
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Preparation");
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Registry)};
                entity_registry_.begin_tick();
            }
            if (player_active) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->prepare_tick(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.prepare_tick(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.prepare_tick(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.prepare_tick(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Spinners)};
                spinners_phase_.prepare_tick(tick_period);
            }
        }
        frame_memory_.reclaim();
        timings.finish_phase();
        timings.set_phase(LevelTelemetryTimingPhase::Thinking);

        /* -------------------------------------------------------------------------------- */
        // Thinking
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Thinking");
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.think(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.think(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.think(tick_period);
            }
            if (player_active) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->think(tick_period);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Spinners)};
                spinners_phase_.think(tick_period);
            }

            if (player_active) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->generate_fire_commands();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.generate_fire_commands();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.generate_fire_commands();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Spinners)};
                spinners_phase_.generate_fire_commands();
            }
        }
        frame_memory_.reclaim();
        timings.finish_phase();
        timings.set_phase(LevelTelemetryTimingPhase::Action);

        /* -------------------------------------------------------------------------------- */
        // Action
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Action");

            // Existing projectiles retain pre-movement collision geometry.
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Lasers)};
                lasers_phase_.simulate(tick_period);
            }
            frame_memory_.reclaim();

            if (player_active) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->apply_movement();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.apply_movement();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Spinners)};
                spinners_phase_.apply_movement();
            }
            frame_memory_.reclaim();

            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Mission)};
                event_manager_.execute_tick(clock_.completed_ticks + 1);
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.commit_spawns();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.execute_fighter_self_destruct_requests();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Lasers)};
                lasers_phase_.commit_spawns();
            }
            frame_memory_.reclaim();

            // Collision observes moved and newly created entities, before resolved deaths.
            publish_entity_state();
            collision::DetectedOverlapsView overlaps;
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::SpatialQueries)};
                auto const authored_spawns{event_manager_.get_spawned_handles()};
                if (authored_spawns.empty()) {
                    overlaps = query_manager_.update(clock_.completed_ticks + 1);
                } else {
                    auto const moved{entity_registry_.get_moved_entities_this_tick()};
                    collision_dirty_entities_.assign(moved.begin(), moved.end());
                    collision_dirty_entities_.insert(collision_dirty_entities_.end(),
                                                     authored_spawns.begin(),
                                                     authored_spawns.end());
                    // Carrier fighters remain queryable but retain their unmoved launch boundary.
                    std::ranges::sort(collision_dirty_entities_);
                    auto const duplicates{std::ranges::unique(collision_dirty_entities_)};
                    collision_dirty_entities_.erase(duplicates.begin(), duplicates.end());
                    overlaps = query_manager_.get_collision_system().update(
                        collision_dirty_entities_, clock_.completed_ticks + 1);
                }
                overlap_handler_.handle(overlaps);
            }

            if (player_active) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Player)};
                player_ship_phase_->resolve_damage_events();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.resolve_damage_events();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.resolve_damage_events();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.resolve_damage_events();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.resolve_fighters_of_dying_capitals();
            }

            // Queue final rows before compaction; include all capital-death consequences.
            publish_entity_state();
            auto const queries_need_refresh{
                !entity_registry_.get_dead_entities_this_frame().empty()};
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.cleanup_entities();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.cleanup_entities();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.cleanup_entities();
            }
            if (queries_need_refresh) {
                auto const timing{timings.scope(SimTelemetryTimingSystem::SpatialQueries)};
                query_manager_.get_collision_system().refresh_queries();
            }
            frame_memory_.reclaim();

            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Mission)};
                mission_manager_.mission_tick();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Capitals)};
                capital_ships_phase_.finish_action();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Fighters)};
                fighters_phase_.finish_action();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Turrets)};
                turrets_phase_.finish_action();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Spinners)};
                spinners_phase_.finish_action();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Lasers)};
                lasers_phase_.finish_action();
            }
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Registry)};
                entity_registry_.end_tick();
            }

            ++clock_.completed_ticks;
            {
                auto const timing{timings.scope(SimTelemetryTimingSystem::Telemetry)};
                level_telemetry_manager_.tick();
            }
            if (on_mission_evaluated) {
                on_mission_evaluated();
            }
            if (on_end_tick) {
                on_end_tick(*this);
            }
            frame_memory_.reset();
        }
        timings.finish_phase();
        timings.record(level_telemetry_manager_);
        profiling::mark_frame("Simulation");

        if (state_ != OrchestratorState::Running) {
            break;
        }
    }

    sample_realtime_telemetry(dt);
}

/* **************************************** */
// Telemetry sampling
/* **************************************** */
void LevelSim::sample_realtime_telemetry(time_type const dt) {
    telemetry_tick_loop_.add_time(dt);
    bool should_sample{};
    while (telemetry_tick_loop_.try_tick()) {
        should_sample = true;
    }
    if (should_sample) {
        level_telemetry_manager_.capture_realtime_sample();
    }
}

auto LevelSim::get_read_view() const -> LevelReadView {
    return {frame_sequence_,
            &clock_,
            capital_ships_simulation_.get_read_view(),
            fighters_simulation_.get_read_view(),
            turrets_simulation_.get_read_view(),
            spinners_simulation_.get_read_view(),
            lasers_simulation_.get_read_view(),
            player_ship_simulation_.has_value()
                ? std::optional<PlayerReadView>{player_ship_simulation_->get_read_view()}
                : std::nullopt,
            &entity_registry_,
            &mission_manager_};
}
} // namespace ioj::sim

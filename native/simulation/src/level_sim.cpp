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
/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(LevelSimInitData& data) {
    std::array<std::uint8_t, static_cast<std::size_t>(ioj::sim::Team::COUNT)> included{};
    auto const included_count{included.size()};
    auto include = [&included](ioj::sim::Team const team) {
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
            data.participating_teams.add(static_cast<ioj::sim::Team>(team_index));
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
    event_manager_.dispatch_tick(0);
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
    capital_ships_simulation_.diagnostics_enabled = data.fighter_diagnostics_enabled;
    fighters_simulation_.diagnostics_enabled = data.fighter_diagnostics_enabled;
    if (data.player.has_value()) {
        configure_player(data.player.value());
    }

    lasers_simulation_.n_preallocated_instances = data.lasers.n_preallocated_instances;
    lasers_simulation_.collision_jobs = data.lasers.collision_jobs;

    capital_ships_simulation_.set_config(data.capital_ships);
    fighters_simulation_.set_config(data.fighters, data.participating_teams);
    fighters_simulation_.fire_dot_product_threshold = data.fighters.fire_dot_product_threshold;
    fighters_simulation_.collision_radius =
        query_manager_.get_entity_type_radius(ioj::sim::EntityType::Fighter);
    fighters_simulation_.fire_point_distance = data.fighter_fire_point_distance;

    turrets_simulation_.set_config(data.turrets);
    turrets_simulation_.search_slice_size = data.turrets.search_slice_size;
    spinners_simulation_.set_config(data.spinners);
}
void LevelSim::configure_player(ioj::sim::player::PlayerSpawnData const& spawn) {
    auto& player{player_ship_simulation_.emplace(
        clock_, entity_registry_, query_manager_, lasers_simulation_)};
    player_ship_phase_.emplace(player);

    player.set_config(spawn.config);
    player.team = spawn.team;

    player.transform = spawn.transform;
    player.body_transform = spawn.body_transform;
    player.left_socket = spawn.left_socket;
    player.right_socket = spawn.right_socket;
    player.middle_socket = spawn.middle_socket;
    player.flight_mode = spawn.flight_mode;
    player.control_mode = spawn.control_mode;
    player.laser_mode = spawn.laser_mode;
    player.laser_fire_rate = spawn.laser_fire_rate;
    player.health = spawn.health;
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
void LevelSim::finalize_telemetry_run(ioj::sim::LevelTelemetryRunEndReason const reason,
                                      std::string detail) {
    level_telemetry_manager_.finalize_interrupted(reason, std::move(detail));
}
void LevelSim::complete_telemetry_run(ioj::sim::LevelTelemetryRunEndReason const reason,
                                      std::optional<ioj::sim::Team> winning_team) {
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
        auto const capture_detailed_timing{level_telemetry_manager_.detailed_timing_enabled() &&
                                           (clock_.completed_ticks % 16) == 0};
        auto const tick_started_at{capture_detailed_timing ? ml::monotonic_seconds() : 0.0};
        std::array<double, SimTelemetryPerformanceWindow::system_count> system_timings;
        for (auto& timing : system_timings) {
            timing = -1.0;
        }

        std::array<double, SimTelemetryPerformanceWindow::phase_count> phase_timings;
        for (auto& timing : phase_timings) {
            timing = -1.0;
        }

        auto phase_started_at{tick_started_at};
        auto finish_phase = [capture_detailed_timing, &phase_timings, &phase_started_at](
                                LevelTelemetryTimingPhase const phase) {
            if (!capture_detailed_timing) {
                return;
            }
            auto const now{ml::monotonic_seconds()};
            phase_timings[static_cast<std::int32_t>(phase)] = now - phase_started_at;
            phase_started_at = now;
        };

        auto measure = [capture_detailed_timing,
                        &system_timings](SimTelemetryTimingSystem const system, auto&& function) {
            if (!capture_detailed_timing) {
                function();
                return;
            }
            auto const started_at{ml::monotonic_seconds()};
            function();
            auto& elapsed{system_timings[static_cast<std::int32_t>(system)]};
            elapsed = std::max(0.0, elapsed) + ml::monotonic_seconds() - started_at;
        };

        auto const player_simulation_is_active{[this] {
            return player_ship_simulation_.has_value() &&
                   player_ship_simulation_->health.is_alive();
        }};
        auto const publish_entity_state{[&] {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::update_entity_registry");
            if (player_simulation_is_active()) {
                player_ship_phase_->update_entity_registry();
            }

            capital_ships_phase_.update_entity_registry();
            fighters_phase_.update_entity_registry();
            turrets_phase_.update_entity_registry();

            measure(SimTelemetryTimingSystem::Registry, [&] { entity_registry_.commit_updates(); });
        }};

        /* -------------------------------------------------------------------------------- */
        // Setup phase
        /* -------------------------------------------------------------------------------- */
        {
            // Clear transient data
            // Assume registry data is stable here
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::begin_tick");

            entity_registry_.begin_tick();
            capital_ships_phase_.begin_tick();
            fighters_phase_.begin_tick();
            turrets_phase_.begin_tick();
            lasers_phase_.begin_tick();

            bool spawned{};
            measure(SimTelemetryTimingSystem::Mission,
                    [&] { spawned = event_manager_.dispatch_tick(clock_.completed_ticks + 1); });
            if (spawned) {
                measure(SimTelemetryTimingSystem::SpatialQueries,
                        [&] { query_manager_.update(clock_.get_completed_ticks() + 1); });
            }
        }
        finish_phase(LevelTelemetryTimingPhase::Setup);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Actor decision phase
        /* -------------------------------------------------------------------------------- */
        // Query target data from registry
        // Queue projectile spawns

        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::update_timers");

            if (player_simulation_is_active()) {
                measure(SimTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->update_timers(tick_period); });
            }
            measure(SimTelemetryTimingSystem::Fighters,
                    [&] { fighters_phase_.update_timers(tick_period); });
            measure(SimTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.update_timers(tick_period); });
            measure(SimTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.update_timers(tick_period); });
            measure(SimTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.update_timers(tick_period); });
        }

        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::make_decisions");
            measure(SimTelemetryTimingSystem::Turrets, [&] { turrets_phase_.make_decisions(); });
            measure(SimTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.make_decisions(); });
            measure(SimTelemetryTimingSystem::Fighters, [&] { fighters_phase_.make_decisions(); });
        }
        finish_phase(LevelTelemetryTimingPhase::Decision);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Simulation phase
        /* -------------------------------------------------------------------------------- */
        {
            // Movement
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::movement");

            if (player_simulation_is_active()) {
                measure(SimTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->move(tick_period); });
            }

            measure(SimTelemetryTimingSystem::Fighters, [&] { fighters_phase_.move(tick_period); });
            measure(SimTelemetryTimingSystem::Spinners, [&] { spinners_phase_.move(tick_period); });
        }
        frame_memory_.reclaim();

        {
            // Queue commands
            // e.g. spawning lasers for the next frame
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::queue_commands");

            if (player_simulation_is_active()) {
                measure(SimTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->queue_commands(); });
            }

            measure(SimTelemetryTimingSystem::Fighters, [&] { fighters_phase_.queue_commands(); });
            measure(SimTelemetryTimingSystem::Turrets, [&] { turrets_phase_.queue_commands(); });
            measure(SimTelemetryTimingSystem::Spinners, [&] { spinners_phase_.queue_commands(); });
        }
        frame_memory_.reclaim();

        {
            // Projectile simulation
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::projectile_simulation");

            measure(SimTelemetryTimingSystem::Lasers, [&] {
                lasers_phase_.simulate(tick_period);
                lasers_phase_.commit_spawns();
            });
        }
        finish_phase(LevelTelemetryTimingPhase::Simulation);
        frame_memory_.reclaim();

        fighters_phase_.commit_spawns();
        publish_entity_state();

        ioj::sim::collision::DetectedOverlapsView detected_overlaps;
        measure(SimTelemetryTimingSystem::SpatialQueries, [&] {
            detected_overlaps = query_manager_.update(clock_.get_completed_ticks() + 1);
        });
        overlap_handler_.handle(detected_overlaps);

        /* -------------------------------------------------------------------------------- */
        // Resolution phase
        /* -------------------------------------------------------------------------------- */
        {
            // Resolve hit events
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::resolve_damage_events");

            if (player_simulation_is_active()) {
                player_ship_phase_->resolve_damage_events();
            }

            measure(SimTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.resolve_damage_events(); });
            measure(SimTelemetryTimingSystem::Fighters,
                    [&] { fighters_phase_.resolve_damage_events(); });
            measure(SimTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.resolve_damage_events(); });
        }

        publish_entity_state();

        {
            // Apply changes from the registry e.g. destroyed targets
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::sync_from_registry");

            capital_ships_phase_.sync_from_registry();
            fighters_phase_.sync_from_registry();
            turrets_phase_.sync_from_registry();
        }

        measure(SimTelemetryTimingSystem::Mission, [&] { mission_manager_.mission_tick(); });

        finish_phase(LevelTelemetryTimingPhase::Resolution);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // End phase
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::end_tick");

            capital_ships_phase_.end_tick();
            fighters_phase_.end_tick();
            turrets_phase_.end_tick();
            spinners_phase_.end_tick();
            lasers_phase_.end_tick();

            measure(SimTelemetryTimingSystem::Registry, [&] { entity_registry_.end_tick(); });
        }
        finish_phase(LevelTelemetryTimingPhase::End);

        ++clock_.completed_ticks;

        auto const telemetry_started_at{capture_detailed_timing ? ml::monotonic_seconds() : 0.0};
        level_telemetry_manager_.tick();
        if (capture_detailed_timing) {
            system_timings[static_cast<std::int32_t>(SimTelemetryTimingSystem::Telemetry)] =
                ml::monotonic_seconds() - telemetry_started_at;
        }

        if (on_mission_evaluated) {
            on_mission_evaluated();
        }

        if (on_end_tick) {
            on_end_tick(*this);
        }

        if (capture_detailed_timing) {
            level_telemetry_manager_.record_simulation_tick_timing(
                ml::monotonic_seconds() - tick_started_at, system_timings, phase_timings);
        }

        frame_memory_.reset();
        ioj::sim::profiling::mark_frame("Simulation");

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

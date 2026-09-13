#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/monotonic_clock.h>
#include <sandbox/simulation/simulation/LevelSimulation.h>
#include <span>
#include <thread>
#include <vector>

namespace ml::level_simulation {
/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(FLevelSimulationInitData& data) {
    std::array<std::uint8_t, static_cast<std::size_t>(ml::simulation::Team::COUNT)> included{};
    auto const included_count{included.size()};
    auto include = [&included](ml::simulation::Team const team) {
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
        for (auto const team : data.capital_spawns.teams) {
            include(team);
        }
        for (auto const team : data.turret_spawns.teams) {
            include(team);
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
            data.participating_teams.add(static_cast<ml::simulation::Team>(team_index));
        }
    }
}

/* **************************************** */
// Legacy initialization
/* **************************************** */
// Legacy initialization supplies spawn arrays instead of compiled level events.
static auto make_legacy_level_initialisation(FLevelSimulationInitData const& data)
    -> ml::FCompiledLevelEvents {
    ml::FCompiledLevelEvents compiled;
    auto& initialisation{compiled.initialisation};
    auto& initial_spawns{compiled.initial_spawns};
    auto const player_offset{data.player.has_value() ? 1 : 0};
    auto const capital_count{data.capital_spawns.num()};
    auto const turret_count{data.turret_spawns.num()};
    initialisation.entity_count = player_offset + capital_count + turret_count;
    initialisation.player_entity_index = data.player.has_value() ? 0 : -1;

    initial_spawns.capital_spawns.add_uninitialised(capital_count);
    for (std::int32_t i{}; i < capital_count; ++i) {
        auto const entity_index{player_offset + i};
        auto const target_index{
            (static_cast<std::size_t>(i) < data.capital_target_spawn_indices.size())
                ? data.capital_target_spawn_indices[i]
                : -1};
        auto const target_entity_index{
            target_index == FLevelSimulationInitData::player_target_spawn_index
                ? initialisation.player_entity_index
                : (target_index == -1 ? -1 : player_offset + target_index)};
        auto& events{initial_spawns.capital_spawns};
        events.entity_indices[i] = entity_index;
        events.target_entity_indices[i] = target_entity_index;
        events.locations.set(i, data.capital_spawns.locations[i]);
        events.rotations.set(i, data.capital_spawns.rotations[i]);
        events.teams[i] = data.capital_spawns.teams[i];
        events.healths[i] = data.capital_spawns.healths[i];
        events.initial_fighter_spawn_delays[i] = data.capital_spawns.initial_spawn_delays[i];
        events.fighter_spawn_cooldowns[i] = data.capital_spawns.spawn_cooldowns[i];
    }

    initial_spawns.turret_spawns.add_uninitialised(turret_count);
    for (std::int32_t i{}; i < turret_count; ++i) {
        auto const entity_index{player_offset + capital_count + i};
        auto const rotation{(static_cast<std::size_t>(i) < data.turret_transforms.size())
                                ? data.turret_transforms[i].rotator()
                                : ml::simulation::Rotator3d{}};
        auto& events{initial_spawns.turret_spawns};
        events.entity_indices[i] = entity_index;
        events.locations.set(i, data.turret_spawns.locations[i]);
        events.rotations.set(i, ml::simulation::to_float(rotation));
        events.teams[i] = data.turret_spawns.teams[i];
        events.healths[i] = data.turret_spawns.healths[i];
        events.laser_damages[i] = data.turret_spawns.laser_damages[i];
    }
    return compiled;
}
}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
FLevelSimulation::FLevelSimulation(FLevelSimulationInitData data)
    : local_game_memory_{data.game_memory == nullptr ? std::make_unique<FGameMemory>() : nullptr}
    , game_memory_{data.game_memory != nullptr ? data.game_memory : local_game_memory_.get()}
    , frame_memory_{data.frame_memory_capacity_bytes}
    , query_manager_{entity_registry_}
    , overlap_handler_{entity_registry_, data.overlap_response}
    , lasers_simulation_{clock_, entity_registry_, query_manager_, frame_memory_}
    , lasers_phase_{lasers_simulation_}
    , capital_ship_fighters_simulation_{clock_,
                                        entity_registry_,
                                        query_manager_,
                                        lasers_simulation_,
                                        frame_memory_}
    , capital_ship_fighters_phase_{capital_ship_fighters_simulation_}
    , capital_ships_simulation_{entity_registry_,
                                query_manager_,
                                capital_ship_fighters_simulation_,
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
    , event_manager_{capital_ships_simulation_, turrets_simulation_, mission_manager_}
    , level_telemetry_manager_{clock_,
                               entity_registry_,
                               lasers_simulation_,
                               query_manager_,
                               *game_memory_,
                               data.telemetry_history} {
    clock_.initialise(data.clock_settings);
    telemetry_metadata_ = std::move(data.telemetry_metadata);
    ml::level_simulation::finalise_participating_teams(data);

    configure_subsystems(data);
    initialise_spatial_queries(data);
    begin_subsystems(data);

    initialise_events(data);
    event_manager_.dispatch_tick(0);
}
void FLevelSimulation::finish_initialisation() {
    assert(state_ == EOrchestratorState::Uninitialised);

    entity_registry_.commit_updates();
    query_manager_.update(clock_.get_completed_ticks());
    entity_registry_.end_tick();

    // Initialization rebuilds must not contribute to runtime telemetry.
    query_manager_.reset_runtime_telemetry();
    initialise_telemetry();

    event_manager_.configure_mission();
    mission_manager_.begin_play();

    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::start() {
    assert(state_ == EOrchestratorState::Paused);
    telemetry_tick_loop_.initialise();
    state_ = EOrchestratorState::Running;
}
void FLevelSimulation::pause() {
    assert(state_ != EOrchestratorState::Uninitialised);
    telemetry_tick_loop_.initialise();
    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::set_time_scale(time_type scale) {
    assert(scale > 0.0);
    clock_.tick_loop.time_scale = scale;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void FLevelSimulation::configure_subsystems(FLevelSimulationInitData const& data) {
    capital_ships_simulation_.diagnostics_enabled = data.fighter_diagnostics_enabled;
    capital_ship_fighters_simulation_.diagnostics_enabled = data.fighter_diagnostics_enabled;
    if (data.player.has_value()) {
        configure_player(data.player.value());
    }

    lasers_simulation_.n_preallocated_instances = data.lasers.n_preallocated_instances;
    lasers_simulation_.collision_jobs = data.lasers.collision_jobs;

    capital_ships_simulation_.set_config(data.capital_ships);
    capital_ships_simulation_.entity_radius = data.capital_radius;

    capital_ship_fighters_simulation_.set_config(data.fighters, data.participating_teams);
    capital_ship_fighters_simulation_.fire_dot_product_threshold =
        data.fighters.fire_dot_product_threshold;
    capital_ship_fighters_simulation_.collision_radius = data.fighter_radius;
    capital_ship_fighters_simulation_.fire_point_distance = data.fighter_fire_point_distance;

    turrets_simulation_.set_config(data.turrets);
    turrets_simulation_.search_slice_size = data.turrets.search_slice_size;
    turrets_simulation_.entity_radius = data.turret_radius;

    spinners_simulation_.set_config(data.spinners);
    spinners_simulation_.entity_radius = data.spinner_radius;
}
void FLevelSimulation::configure_player(ml::test_space_ship::FPlayerSpawnData const& spawn) {
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
    player.collision_radius = spawn.collision_radius;

    player.flight_mode = spawn.flight_mode;
    player.control_mode = spawn.control_mode;
    player.laser_mode = spawn.laser_mode;
    player.laser_fire_rate = spawn.laser_fire_rate;
    player.health = spawn.health;
}
void FLevelSimulation::initialise_spatial_queries(FLevelSimulationInitData& data) {
    query_manager_.initialise(data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency())));
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(
        std::move(data.static_bounds));
}
void FLevelSimulation::begin_subsystems(FLevelSimulationInitData const& data) {
    if (player_ship_simulation_.has_value()) {
        player_ship_phase_->begin_play();
    }

    capital_ships_phase_.begin_play();
    capital_ship_fighters_phase_.begin_play();
    turrets_phase_.begin_play();

    spinners_simulation_.spawn_instances(
        data.spinner_locations.get_const_view(), data.spinner_yaws, data.spinner_fire_points);
    spinners_phase_.begin_play();
    lasers_phase_.begin_play();
}
void FLevelSimulation::initialise_events(FLevelSimulationInitData& data) {
    if (!data.level_events.initialisation.mission.has_value() &&
        data.level_events.initialisation.entity_count == 0 &&
        (!data.capital_spawns.is_empty() || !data.turret_spawns.is_empty())) {
        data.level_events = ml::level_simulation::make_legacy_level_initialisation(data);
    }

    auto const player_handle{player_ship_simulation_.has_value()
                                 ? player_ship_simulation_->registry_handle
                                 : FRegistryEntityHandle{}};
    event_manager_.initialise(std::move(data.level_events), player_handle);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void FLevelSimulation::initialise_telemetry() {
    level_telemetry_manager_.initialise();
    telemetry_tick_loop_.tick_rate = 4.0;
    telemetry_tick_loop_.time_scale = 1.0;
    telemetry_tick_loop_.initialise();

    if (telemetry_metadata_.has_value()) {
        level_telemetry_manager_.begin_run(std::move(telemetry_metadata_.value()));
        telemetry_metadata_.reset();
    }
}
void FLevelSimulation::finalize_telemetry_run(
    ml::simulation::LevelTelemetryRunEndReason const reason, std::string detail) {
    level_telemetry_manager_.finalize_interrupted(reason, std::move(detail));
}
void FLevelSimulation::complete_telemetry_run(
    ml::simulation::LevelTelemetryRunEndReason const reason,
    std::optional<ml::simulation::Team> winning_team) {
    level_telemetry_manager_.finalize_completed(reason, winning_team);
    state_ = EOrchestratorState::Paused;
}
auto FLevelSimulation::take_mission_result() -> std::optional<FLevelMissionResult> {
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
void FLevelSimulation::advance(time_type const dt) {

    if (state_ != EOrchestratorState::Running) {
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
        auto const capture_detailed_timing{level_telemetry_manager_.detailed_timing_enabled() &&
                                           (clock_.completed_ticks % 16) == 0};
        auto const tick_started_at{capture_detailed_timing ? ml::monotonic_seconds() : 0.0};
        std::array<double, FSimulationTelemetryPerformanceWindow::system_count> system_timings;
        for (auto& timing : system_timings) {
            timing = -1.0;
        }

        std::array<double, FSimulationTelemetryPerformanceWindow::phase_count> phase_timings;
        for (auto& timing : phase_timings) {
            timing = -1.0;
        }

        auto phase_started_at{tick_started_at};
        auto finish_phase = [capture_detailed_timing, &phase_timings, &phase_started_at](
                                ELevelTelemetryTimingPhase const phase) {
            if (!capture_detailed_timing) {
                return;
            }
            auto const now{ml::monotonic_seconds()};
            phase_timings[static_cast<std::int32_t>(phase)] = now - phase_started_at;
            phase_started_at = now;
        };

        auto measure = [capture_detailed_timing, &system_timings](
                           ESimulationTelemetryTimingSystem const system, auto&& function) {
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
            if (player_simulation_is_active()) {
                player_ship_phase_->update_entity_registry();
            }

            capital_ships_phase_.update_entity_registry();
            capital_ship_fighters_phase_.update_entity_registry();
            turrets_phase_.update_entity_registry();

            measure(ESimulationTelemetryTimingSystem::Registry,
                    [&] { entity_registry_.commit_updates(); });
        }};

        /* -------------------------------------------------------------------------------- */
        // Setup phase
        /* -------------------------------------------------------------------------------- */
        {
            // Clear transient data
            // Assume registry data is stable here

            entity_registry_.begin_tick();
            capital_ships_phase_.begin_tick();
            capital_ship_fighters_phase_.begin_tick();
            turrets_phase_.begin_tick();
            lasers_phase_.begin_tick();

            bool spawned{};
            measure(ESimulationTelemetryTimingSystem::Mission,
                    [&] { spawned = event_manager_.dispatch_tick(clock_.completed_ticks + 1); });
            if (spawned) {
                measure(ESimulationTelemetryTimingSystem::SpatialQueries,
                        [&] { query_manager_.update(clock_.get_completed_ticks() + 1); });
            }
        }
        finish_phase(ELevelTelemetryTimingPhase::Setup);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Actor decision phase
        /* -------------------------------------------------------------------------------- */
        // Query target data from registry
        // Queue projectile spawns

        {

            if (player_simulation_is_active()) {
                measure(ESimulationTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->update_timers(clock_.tick_loop.tick_period); });
            }
            measure(ESimulationTelemetryTimingSystem::Fighters, [&] {
                capital_ship_fighters_phase_.update_timers(clock_.tick_loop.tick_period);
            });
            measure(ESimulationTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.update_timers(clock_.tick_loop.tick_period); });
            measure(ESimulationTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.update_timers(clock_.tick_loop.tick_period); });
            measure(ESimulationTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.update_timers(clock_.tick_loop.tick_period); });
        }

        {
            measure(ESimulationTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.make_decisions(); });
            measure(ESimulationTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.make_decisions(); });
            measure(ESimulationTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.make_decisions(); });
        }
        finish_phase(ELevelTelemetryTimingPhase::Decision);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Simulation phase
        /* -------------------------------------------------------------------------------- */
        {
            // Movement

            if (player_simulation_is_active()) {
                measure(ESimulationTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->move(clock_.tick_loop.tick_period); });
            }

            measure(ESimulationTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.move(clock_.tick_loop.tick_period); });
            measure(ESimulationTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.move(clock_.tick_loop.tick_period); });
        }
        frame_memory_.reclaim();

        {
            // Queue commands
            // e.g. spawning lasers for the next frame

            if (player_simulation_is_active()) {
                measure(ESimulationTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_->queue_commands(); });
            }

            measure(ESimulationTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.queue_commands(); });
            measure(ESimulationTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.queue_commands(); });
            measure(ESimulationTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.queue_commands(); });
        }
        frame_memory_.reclaim();

        {
            // Projectile simulation

            measure(ESimulationTelemetryTimingSystem::Lasers, [&] {
                lasers_phase_.simulate(clock_.tick_loop.tick_period);
                lasers_phase_.commit_spawns();
            });
        }
        finish_phase(ELevelTelemetryTimingPhase::Simulation);
        frame_memory_.reclaim();

        capital_ship_fighters_phase_.commit_spawns();
        publish_entity_state();

        ml::ioj::FDetectedOverlapsView detected_overlaps;
        measure(ESimulationTelemetryTimingSystem::SpatialQueries, [&] {
            detected_overlaps = query_manager_.update(clock_.get_completed_ticks() + 1);
        });
        overlap_handler_.handle(detected_overlaps);

        /* -------------------------------------------------------------------------------- */
        // Resolution phase
        /* -------------------------------------------------------------------------------- */
        {
            // Resolve hit events

            if (player_simulation_is_active()) {
                player_ship_phase_->resolve_damage_events();
            }

            measure(ESimulationTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.resolve_damage_events(); });
            measure(ESimulationTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.resolve_damage_events(); });
            measure(ESimulationTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.resolve_damage_events(); });
        }

        publish_entity_state();

        {
            // Apply changes from the registry e.g. destroyed targets

            capital_ships_phase_.sync_from_registry();
            capital_ship_fighters_phase_.sync_from_registry();
            turrets_phase_.sync_from_registry();
        }

        measure(ESimulationTelemetryTimingSystem::Mission,
                [&] { mission_manager_.mission_tick(); });

        finish_phase(ELevelTelemetryTimingPhase::Resolution);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // End phase
        /* -------------------------------------------------------------------------------- */
        {

            capital_ships_phase_.end_tick();
            capital_ship_fighters_phase_.end_tick();
            turrets_phase_.end_tick();
            spinners_phase_.end_tick();
            lasers_phase_.end_tick();

            measure(ESimulationTelemetryTimingSystem::Registry,
                    [&] { entity_registry_.end_tick(); });
        }
        finish_phase(ELevelTelemetryTimingPhase::End);

        ++clock_.completed_ticks;

        auto const telemetry_started_at{capture_detailed_timing ? ml::monotonic_seconds() : 0.0};
        level_telemetry_manager_.tick();
        if (capture_detailed_timing) {
            system_timings[static_cast<std::int32_t>(ESimulationTelemetryTimingSystem::Telemetry)] =
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

        if (state_ != EOrchestratorState::Running) {
            break;
        }
    }

    sample_realtime_telemetry(dt);
}

/* **************************************** */
// Telemetry sampling
/* **************************************** */
void FLevelSimulation::sample_realtime_telemetry(time_type const dt) {
    telemetry_tick_loop_.add_time(dt);
    bool should_sample{};
    while (telemetry_tick_loop_.try_tick()) {
        should_sample = true;
    }
    if (should_sample) {
        level_telemetry_manager_.capture_realtime_sample();
    }
}

auto FLevelSimulation::get_read_view() const -> FLevelReadView {
    return {frame_sequence_,
            &clock_,
            capital_ships_simulation_.get_read_view(),
            capital_ship_fighters_simulation_.get_read_view(),
            turrets_simulation_.get_read_view(),
            spinners_simulation_.get_read_view(),
            lasers_simulation_.get_read_view(),
            player_ship_simulation_.has_value()
                ? std::optional<FPlayerReadView>{player_ship_simulation_->get_read_view()}
                : std::nullopt,
            &entity_registry_,
            &mission_manager_};
}

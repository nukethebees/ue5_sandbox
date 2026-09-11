#include <HAL/PlatformMisc.h>
#include <HAL/PlatformTime.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml::level_simulation {
/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(FLevelSimulationInitData& data) {
    TStaticArray<uint8, static_cast<int32>(ETestTeam::COUNT)> included{};
    auto const included_count{included.Num()};
    auto include = [&included, included_count](ETestTeam const team) {
        auto const team_index{static_cast<int32>(team)};
        if (team_index >= 0 && team_index < included_count) {
            included[team_index] = 1;
        } else {
            ensureAlwaysMsgf(false, TEXT("Ignoring invalid participating team %d"), team_index);
        }
    };

    for (auto const team : data.participating_teams) {
        include(team);
    }
    if (data.participating_teams.is_empty()) {
        if (data.player.IsSet()) {
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
    for (int32 team_index{}; team_index < included_count; ++team_index) {
        if (included[team_index] != 0) {
            data.participating_teams.add(static_cast<ETestTeam>(team_index));
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
    auto const player_offset{data.player.IsSet() ? 1 : 0};
    auto const capital_count{data.capital_spawns.num()};
    auto const turret_count{data.turret_spawns.num()};
    initialisation.entity_count = player_offset + capital_count + turret_count;
    initialisation.player_entity_index = data.player.IsSet() ? 0 : INDEX_NONE;

    initial_spawns.capital_spawns.add_uninitialised(capital_count);
    for (int32 i{}; i < capital_count; ++i) {
        auto const entity_index{player_offset + i};
        auto const target_index{data.capital_target_spawn_indices.IsValidIndex(i)
                                    ? data.capital_target_spawn_indices[i]
                                    : INDEX_NONE};
        auto const target_entity_index{
            target_index == FLevelSimulationInitData::player_target_spawn_index
                ? initialisation.player_entity_index
                : (target_index == INDEX_NONE ? INDEX_NONE : player_offset + target_index)};
        initial_spawns.capital_spawns.set(
            i,
            entity_index,
            target_entity_index,
            data.capital_spawns.locations[i],
            FRotator3f{ml::get_rotator3d(data.capital_spawns.rotations, i)},
            data.capital_spawns.teams[i],
            data.capital_spawns.healths[i],
            data.capital_spawns.initial_spawn_delays[i],
            data.capital_spawns.spawn_cooldowns[i]);
    }

    initial_spawns.turret_spawns.add_uninitialised(turret_count);
    for (int32 i{}; i < turret_count; ++i) {
        auto const entity_index{player_offset + capital_count + i};
        auto const rotation{data.turret_transforms.IsValidIndex(i)
                                ? data.turret_transforms[i].Rotator()
                                : FRotator::ZeroRotator};
        initial_spawns.turret_spawns.set(i,
                                         entity_index,
                                         data.turret_spawns.locations[i],
                                         FRotator3f{rotation},
                                         data.turret_spawns.teams[i],
                                         data.turret_spawns.healths[i],
                                         data.turret_spawns.laser_damages[i]);
    }
    return compiled;
}
}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
FLevelSimulation::FLevelSimulation(FLevelSimulationInitData data)
    : local_game_memory_{data.game_memory == nullptr ? MakeUnique<FGameMemory>() : nullptr}
    , game_memory_{data.game_memory != nullptr ? data.game_memory : local_game_memory_.Get()}
    , frame_memory_{data.frame_memory_capacity_bytes}
    , query_manager_{entity_registry_}
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
    telemetry_metadata_ = MoveTemp(data.telemetry_metadata);
    ml::level_simulation::finalise_participating_teams(data);

    configure_subsystems(data);
    initialise_spatial_queries(data);
    begin_subsystems(data);

    initialise_events(data);
    event_manager_.dispatch_tick(0);
}
void FLevelSimulation::finish_initialisation() {
    check(state_ == EOrchestratorState::Uninitialised);

    entity_registry_.commit_updates();
    query_manager_.update();
    entity_registry_.end_tick();

    // Initialization rebuilds must not contribute to runtime telemetry.
    query_manager_.reset_runtime_telemetry();
    initialise_telemetry();

    event_manager_.configure_mission();
    mission_manager_.begin_play();

    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::start() {
    check(state_ == EOrchestratorState::Paused);
    telemetry_tick_loop_.initialise();
    state_ = EOrchestratorState::Running;
}
void FLevelSimulation::pause() {
    check(state_ != EOrchestratorState::Uninitialised);
    telemetry_tick_loop_.initialise();
    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::set_time_scale(time_type scale) {
    check(scale > 0.0);
    clock_.tick_loop.time_scale = scale;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void FLevelSimulation::configure_subsystems(FLevelSimulationInitData const& data) {
    if (data.player.IsSet()) {
        configure_player(data.player.GetValue());
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
    auto& player{player_ship_simulation_.Emplace(
        clock_, entity_registry_, query_manager_, lasers_simulation_)};
    player_ship_phase_.Emplace(player);

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
void FLevelSimulation::initialise_spatial_queries(FLevelSimulationInitData const& data) {
    query_manager_.initialise(data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads()));
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(data.static_bounds);
}
void FLevelSimulation::begin_subsystems(FLevelSimulationInitData const& data) {
    if (player_ship_simulation_.IsSet()) {
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
    if (!data.level_events.initialisation.mission.IsSet() &&
        data.level_events.initialisation.entity_count == 0 &&
        (!data.capital_spawns.is_empty() || !data.turret_spawns.is_empty())) {
        data.level_events = ml::level_simulation::make_legacy_level_initialisation(data);
    }

    auto const player_handle{player_ship_simulation_.IsSet()
                                 ? player_ship_simulation_->registry_handle
                                 : FRegistryEntityHandle{}};
    event_manager_.initialise(MoveTemp(data.level_events), player_handle);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void FLevelSimulation::initialise_telemetry() {
    level_telemetry_manager_.initialise();
    telemetry_tick_loop_.tick_rate = 4.0;
    telemetry_tick_loop_.time_scale = 1.0;
    telemetry_tick_loop_.initialise();

    if (telemetry_metadata_.IsSet()) {
        level_telemetry_manager_.begin_run(MoveTemp(telemetry_metadata_.GetValue()));
        telemetry_metadata_.Reset();
    }
}
void FLevelSimulation::finalize_telemetry_run(ELevelTelemetryRunEndReason const reason,
                                              FString detail) {
    level_telemetry_manager_.finalize_interrupted(reason, MoveTemp(detail));
}
void FLevelSimulation::complete_telemetry_run(ELevelTelemetryRunEndReason const reason,
                                              TOptional<ETestTeam> winning_team) {
    level_telemetry_manager_.finalize_completed(reason, winning_team);
    state_ = EOrchestratorState::Paused;
}
auto FLevelSimulation::take_mission_result() -> TOptional<FLevelMissionResult> {
    auto result{mission_manager_.take_result()};
    if (!result.IsSet()) {
        return NullOpt;
    }

    level_telemetry_manager_.mark_mission_terminal(*result);
    return result;
}

/* **************************************** */
// Simulation
/* **************************************** */
void FLevelSimulation::advance(time_type const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance);

    if (state_ != EOrchestratorState::Running) {
        return;
    }

    ++frame_sequence_;
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    level_telemetry_manager_.observe_frame(dt);
    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
        auto const capture_detailed_timing{level_telemetry_manager_.detailed_timing_enabled() &&
                                           (clock_.completed_ticks % 16) == 0};
        auto const tick_started_at{capture_detailed_timing ? FPlatformTime::Seconds() : 0.0};
        TStaticArray<double, FSimulationTelemetryPerformanceWindow::system_count> system_timings;
        for (auto& timing : system_timings) {
            timing = -1.0;
        }

        TStaticArray<double, FSimulationTelemetryPerformanceWindow::phase_count> phase_timings;
        for (auto& timing : phase_timings) {
            timing = -1.0;
        }

        auto phase_started_at{tick_started_at};
        auto finish_phase = [capture_detailed_timing, &phase_timings, &phase_started_at](
                                ELevelTelemetryTimingPhase const phase) {
            if (!capture_detailed_timing) {
                return;
            }
            auto const now{FPlatformTime::Seconds()};
            phase_timings[static_cast<int32>(phase)] = now - phase_started_at;
            phase_started_at = now;
        };

        auto measure = [capture_detailed_timing, &system_timings](
                           ESimulationTelemetryTimingSystem const system, auto&& function) {
            if (!capture_detailed_timing) {
                function();
                return;
            }
            auto const started_at{FPlatformTime::Seconds()};
            function();
            auto& elapsed{system_timings[static_cast<int32>(system)]};
            elapsed = FMath::Max(0.0, elapsed) + FPlatformTime::Seconds() - started_at;
        };

        auto const player_simulation_is_active{[this] {
            return player_ship_simulation_.IsSet() && player_ship_simulation_->health.is_alive();
        }};

        /* -------------------------------------------------------------------------------- */
        // Setup phase
        /* -------------------------------------------------------------------------------- */
        {
            // Clear transient data
            // Assume registry data is stable here
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::begin_tick);

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
                        [&] { query_manager_.update(); });
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
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::update_timers);

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
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::make_decisions);
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
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::movement);

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
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::queue_commands);

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
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::projectile_simulation);

            measure(ESimulationTelemetryTimingSystem::Lasers, [&] {
                lasers_phase_.simulate(clock_.tick_loop.tick_period);
                lasers_phase_.commit_spawns();
            });
        }
        finish_phase(ELevelTelemetryTimingPhase::Simulation);
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Resolution phase
        /* -------------------------------------------------------------------------------- */
        {
            // Resolve hit events
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::resolve_damage_events);

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

        {
            // Send updates to the registry
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::update_entity_registry);

            if (player_simulation_is_active()) {
                player_ship_phase_->update_entity_registry();
            }

            capital_ships_phase_.update_entity_registry();
            capital_ship_fighters_phase_.update_entity_registry();
            turrets_phase_.update_entity_registry();
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::commit_updates);

            measure(ESimulationTelemetryTimingSystem::Registry,
                    [&] { entity_registry_.commit_updates(); });
        }

        {
            // Apply changes from the registry e.g. destroyed targets
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::sync_from_registry);

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
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::end_tick);

            capital_ships_phase_.end_tick();
            capital_ship_fighters_phase_.end_tick();
            turrets_phase_.end_tick();
            spinners_phase_.end_tick();
            lasers_phase_.end_tick();

            measure(ESimulationTelemetryTimingSystem::SpatialQueries,
                    [&] { query_manager_.update(); });
            measure(ESimulationTelemetryTimingSystem::Registry,
                    [&] { entity_registry_.end_tick(); });
        }
        finish_phase(ELevelTelemetryTimingPhase::End);

        ++clock_.completed_ticks;

        auto const telemetry_started_at{capture_detailed_timing ? FPlatformTime::Seconds() : 0.0};
        level_telemetry_manager_.tick();
        if (capture_detailed_timing) {
            system_timings[static_cast<int32>(ESimulationTelemetryTimingSystem::Telemetry)] =
                FPlatformTime::Seconds() - telemetry_started_at;
        }

        if (on_mission_evaluated) {
            on_mission_evaluated();
        }

        if (on_end_tick) {
            on_end_tick(*this);
        }

        if (capture_detailed_timing) {
            level_telemetry_manager_.record_simulation_tick_timing(
                FPlatformTime::Seconds() - tick_started_at, system_timings, phase_timings);
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
            player_ship_simulation_.IsSet()
                ? TOptional<FPlayerReadView>{player_ship_simulation_->get_read_view()}
                : NullOpt,
            &entity_registry_,
            &mission_manager_};
}

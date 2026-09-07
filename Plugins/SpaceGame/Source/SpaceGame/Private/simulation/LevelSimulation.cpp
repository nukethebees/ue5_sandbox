#include <HAL/PlatformMisc.h>
#include <HAL/PlatformTime.h>
#include <SpaceGame/simulation/LevelSimulation.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <CoreGlobals.h>

namespace {
auto make_legacy_level_initialisation(FLevelSimulationInitData const& data)
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

FLevelSimulation::FLevelSimulation(FLevelSimulationInitData data,
                                   FLevelPresentationResources const* presentation) {
    clock_.initialise(data.clock_settings);
    telemetry_metadata_ = MoveTemp(data.telemetry_metadata);
    if (data.player.IsSet()) {
        auto const& spawn{data.player.GetValue()};
        auto& player{player_ship_simulation_.Emplace()};
        player.set_config(spawn.config);
        player.team = spawn.team;
        player.transform = spawn.transform;
        player.visual_transform = spawn.visual_transform;
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
    lasers_simulation_.n_preallocated_instances = data.lasers.n_preallocated_instances;
    lasers_simulation_.collision_jobs = data.lasers.collision_jobs;
    capital_ships_simulation_.set_config(data.capital_ships);
    capital_ship_fighters_simulation_.set_config(data.fighters);
    capital_ship_fighters_simulation_.fire_dot_product_threshold =
        data.fighters.fire_dot_product_threshold;
    turrets_simulation_.set_config(data.turrets);
    turrets_simulation_.search_slice_size = data.turrets.search_slice_size;
    spinners_simulation_.set_config(data.spinners);
    capital_ships_simulation_.entity_radius = data.capital_radius;
    capital_ship_fighters_simulation_.collision_radius = data.fighter_radius;
    capital_ship_fighters_simulation_.fire_point_distance = data.fighter_fire_point_distance;
    turrets_simulation_.entity_radius = data.turret_radius;
    spinners_simulation_.entity_radius = data.spinner_radius;
    bind_simulation_dependencies();
    query_manager_.initialise(
        entity_registry_, data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads()));
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(data.static_bounds);

    if (player_ship_simulation_.IsSet()) {
        player_ship_phase_.begin_play();
    }
    capital_ships_phase_.begin_play();
    capital_ship_fighters_phase_.begin_play();
    turrets_phase_.begin_play();
    spinners_simulation_.spawn_instances(
        data.spinner_locations.get_const_view(), data.spinner_yaws, data.spinner_fire_points);
    spinners_phase_.begin_play();
    lasers_phase_.begin_play();

    if (!data.level_events.initialisation.mission.IsSet() &&
        data.level_events.initialisation.entity_count == 0 &&
        (!data.capital_spawns.is_empty() || !data.turret_spawns.is_empty())) {
        data.level_events = make_legacy_level_initialisation(data);
    }
    auto const player_handle{player_ship_simulation_.IsSet()
                                 ? player_ship_simulation_->registry_handle
                                 : FRegistryEntityHandle{}};
    event_manager_.initialise(MoveTemp(data.level_events),
                              capital_ships_simulation_,
                              turrets_simulation_,
                              mission_manager_,
                              player_handle);
    event_manager_.dispatch_tick(0);
    if (presentation) {
        check(presentation->is_valid());
        presentation_.Emplace(*presentation, *this, MoveTemp(data.turret_transforms));
    }
}

void FLevelSimulation::finish_initialisation() {
    check(state_ == EOrchestratorState::Uninitialised);
    entity_registry_.commit_updates();
    entity_registry_.end_tick();
    query_manager_.update();
    query_manager_.reset_runtime_telemetry();
    level_telemetry_manager_.initialise(
        clock_, entity_registry_, lasers_simulation_, query_manager_);
    telemetry_tick_loop_.tick_rate = 4.0;
    telemetry_tick_loop_.time_scale = 1.0;
    telemetry_tick_loop_.initialise();
    if (telemetry_metadata_.IsSet()) {
        level_telemetry_manager_.begin_run(MoveTemp(telemetry_metadata_.GetValue()));
        telemetry_metadata_.Reset();
    }
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

void FLevelSimulation::finalize_telemetry_run(ELevelTelemetryRunEndReason const reason,
                                              FString detail) {
    level_telemetry_manager_.finalize_interrupted(reason, MoveTemp(detail));
    persist_finalized_telemetry_run();
}

void FLevelSimulation::complete_telemetry_run(ELevelTelemetryRunEndReason const reason,
                                              TOptional<ETestTeam> winning_team) {
    level_telemetry_manager_.finalize_completed(reason, winning_team);
    state_ = EOrchestratorState::Paused;
    persist_finalized_telemetry_run();
}

auto FLevelSimulation::take_mission_result() -> TOptional<FLevelMissionResult> {
    auto result{mission_manager_.take_result()};
    if (!result.IsSet()) {
        return NullOpt;
    }

    level_telemetry_manager_.mark_mission_terminal(*result);
    return result;
}

void FLevelSimulation::bind_simulation_dependencies() {

    if (player_ship_simulation_.IsSet()) {
        player_ship_phase_.bind(player_ship_simulation_.GetValue());
    }
    lasers_phase_.bind(lasers_simulation_);
    capital_ships_phase_.bind(capital_ships_simulation_);
    capital_ship_fighters_phase_.bind(capital_ship_fighters_simulation_);
    turrets_phase_.bind(turrets_simulation_);
    spinners_phase_.bind(spinners_simulation_);

    capital_ships_simulation_.bind_fighters(capital_ship_fighters_simulation_);

    if (player_ship_simulation_.IsSet()) {
        player_ship_simulation_->bind_simulation_clock(clock_);
    }
    lasers_simulation_.bind_simulation_clock(clock_);
    capital_ship_fighters_simulation_.bind_simulation_clock(clock_);
    turrets_simulation_.bind_simulation_clock(clock_);
    spinners_simulation_.bind_simulation_clock(clock_);
    mission_manager_.bind_simulation_clock(clock_);

    if (player_ship_simulation_.IsSet()) {
        player_ship_simulation_->set_entity_registry(entity_registry_);
        player_ship_simulation_->set_spatial_query_manager(query_manager_);
        player_ship_simulation_->set_lasers(lasers_simulation_);
    }

    capital_ships_simulation_.set_entity_registry(entity_registry_);
    turrets_simulation_.set_entity_registry(entity_registry_);
    spinners_simulation_.set_entity_registry(entity_registry_);
    capital_ship_fighters_simulation_.set_entity_registry(entity_registry_);
    lasers_simulation_.set_entity_registry(entity_registry_);
    mission_manager_.set_entity_registry(entity_registry_);

    lasers_simulation_.set_spatial_query_manager(query_manager_);
    capital_ships_simulation_.set_spatial_query_manager(query_manager_);
    capital_ship_fighters_simulation_.set_spatial_query_manager(query_manager_);
    turrets_simulation_.set_spatial_query_manager(query_manager_);

    capital_ship_fighters_simulation_.set_laser_simulation(lasers_simulation_);
    turrets_simulation_.set_laser_simulation(lasers_simulation_);
    spinners_simulation_.set_laser_simulation(lasers_simulation_);
}

void FLevelSimulation::advance(time_type const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance);

    if (state_ != EOrchestratorState::Running) {
        return;
    }

    level_telemetry_manager_.observe_frame(dt);
    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
        auto const capture_detailed_timing{level_telemetry_manager_.detailed_timing_enabled() &&
                                           (clock_.completed_ticks % 16) == 0};
        auto const tick_started_at{capture_detailed_timing ? FPlatformTime::Seconds() : 0.0};
        TStaticArray<double, FLevelTelemetryPerformanceWindow::system_count> system_timings;
        for (auto& timing : system_timings) {
            timing = -1.0;
        }
        TStaticArray<double, FLevelTelemetryPerformanceWindow::phase_count> phase_timings;
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
                           ELevelTelemetryTimingSystem const system, auto&& function) {
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

            capital_ships_phase_.begin_tick();
            capital_ship_fighters_phase_.begin_tick();
            turrets_phase_.begin_tick();
            lasers_phase_.begin_tick();

            bool spawned{};
            measure(ELevelTelemetryTimingSystem::Mission,
                    [&] { spawned = event_manager_.dispatch_tick(clock_.completed_ticks + 1); });
            if (spawned) {
                measure(ELevelTelemetryTimingSystem::SpatialQueries,
                        [&] { query_manager_.update(); });
            }
        }
        finish_phase(ELevelTelemetryTimingPhase::Setup);

        /* -------------------------------------------------------------------------------- */
        // Actor decision phase
        /* -------------------------------------------------------------------------------- */
        // Query target data from registry
        // Queue projectile spawns

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::update_timers);

            if (player_simulation_is_active()) {
                measure(ELevelTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_.update_timers(clock_.tick_loop.tick_period); });
            }
            measure(ELevelTelemetryTimingSystem::Fighters, [&] {
                capital_ship_fighters_phase_.update_timers(clock_.tick_loop.tick_period);
            });
            measure(ELevelTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.update_timers(clock_.tick_loop.tick_period); });
            measure(ELevelTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.update_timers(clock_.tick_loop.tick_period); });
            measure(ELevelTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.update_timers(clock_.tick_loop.tick_period); });
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::make_decisions);
            measure(ELevelTelemetryTimingSystem::Turrets, [&] { turrets_phase_.make_decisions(); });
            measure(ELevelTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.make_decisions(); });
            measure(ELevelTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.make_decisions(); });
        }
        finish_phase(ELevelTelemetryTimingPhase::Decision);

        /* -------------------------------------------------------------------------------- */
        // Simulation phase
        /* -------------------------------------------------------------------------------- */
        {
            // Movement
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::movement);

            if (player_simulation_is_active()) {
                measure(ELevelTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_.move(clock_.tick_loop.tick_period); });
            }

            measure(ELevelTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.move(clock_.tick_loop.tick_period); });
            measure(ELevelTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.move(clock_.tick_loop.tick_period); });
        }

        {
            // Queue commands
            // e.g. spawning lasers for the next frame
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::queue_commands);

            if (player_simulation_is_active()) {
                measure(ELevelTelemetryTimingSystem::Player,
                        [&] { player_ship_phase_.queue_commands(); });
            }

            measure(ELevelTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.queue_commands(); });
            measure(ELevelTelemetryTimingSystem::Turrets, [&] { turrets_phase_.queue_commands(); });
            measure(ELevelTelemetryTimingSystem::Spinners,
                    [&] { spinners_phase_.queue_commands(); });
        }

        {
            // Projectile simulation
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::projectile_simulation);

            measure(ELevelTelemetryTimingSystem::Lasers, [&] {
                lasers_phase_.simulate(clock_.tick_loop.tick_period);
                lasers_phase_.commit_spawns();
            });
        }
        finish_phase(ELevelTelemetryTimingPhase::Simulation);

        /* -------------------------------------------------------------------------------- */
        // Resolution phase
        /* -------------------------------------------------------------------------------- */
        {
            // Resolve hit events
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::resolve_damage_events);

            if (player_simulation_is_active()) {
                player_ship_phase_.resolve_damage_events();
                if (player_ship_simulation_->consume_death_notification() &&
                    presentation_.IsSet()) {
                    presentation_->handle_player_death();
                }
            }

            measure(ELevelTelemetryTimingSystem::Capitals,
                    [&] { capital_ships_phase_.resolve_damage_events(); });
            measure(ELevelTelemetryTimingSystem::Fighters,
                    [&] { capital_ship_fighters_phase_.resolve_damage_events(); });
            measure(ELevelTelemetryTimingSystem::Turrets,
                    [&] { turrets_phase_.resolve_damage_events(); });
        }

        {
            // Send updates to the registry
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::update_entity_registry);

            if (player_simulation_is_active()) {
                player_ship_phase_.update_entity_registry();
            }

            capital_ships_phase_.update_entity_registry();
            capital_ship_fighters_phase_.update_entity_registry();
            turrets_phase_.update_entity_registry();
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::commit_updates);

            measure(ELevelTelemetryTimingSystem::Registry,
                    [&] { entity_registry_.commit_updates(); });
        }

        {
            // Apply changes from the registry e.g. destroyed targets
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::sync_from_registry);

            capital_ships_phase_.sync_from_registry();
            capital_ship_fighters_phase_.sync_from_registry();
            turrets_phase_.sync_from_registry();
        }

        measure(ELevelTelemetryTimingSystem::Mission, [&] { mission_manager_.mission_tick(); });

        if (presentation_.IsSet()) {
            measure(ELevelTelemetryTimingSystem::Presentation,
                    [&] { presentation_->update_visual_data(clock_.tick_loop.tick_period); });
        }
        finish_phase(ELevelTelemetryTimingPhase::Resolution);

        /* -------------------------------------------------------------------------------- */
        // End phase
        /* -------------------------------------------------------------------------------- */
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::end_tick);

            capital_ships_phase_.end_tick();
            if (presentation_.IsSet()) {
                presentation_->capital_ships.end_tick_presentation();
            }
            capital_ship_fighters_phase_.end_tick();
            if (presentation_.IsSet()) {
                presentation_->capital_ship_fighters.end_tick_presentation();
            }
            turrets_phase_.end_tick();
            if (presentation_.IsSet()) {
                presentation_->turrets.end_tick_presentation();
            }
            spinners_phase_.end_tick();
            if (presentation_.IsSet()) {
                presentation_->spinners.end_tick_presentation();
            }
            lasers_phase_.end_tick();
            if (presentation_.IsSet()) {
                presentation_->lasers.end_tick_presentation();
            }
            measure(ELevelTelemetryTimingSystem::Registry, [&] { entity_registry_.end_tick(); });
            measure(ELevelTelemetryTimingSystem::SpatialQueries, [&] { query_manager_.update(); });
        }
        finish_phase(ELevelTelemetryTimingPhase::End);

        ++clock_.completed_ticks;
        auto const telemetry_started_at{capture_detailed_timing ? FPlatformTime::Seconds() : 0.0};
        level_telemetry_manager_.tick();
        if (capture_detailed_timing) {
            system_timings[static_cast<int32>(ELevelTelemetryTimingSystem::Telemetry)] =
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
        if (state_ != EOrchestratorState::Running) {
            break;
        }
    }
    sample_realtime_telemetry(dt);
    persist_finalized_telemetry_run();
}

void FLevelSimulation::commit_presentation(time_type const dt) {
    if (presentation_.IsSet()) {
        presentation_->commit_visual_data(dt);
    }
}

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

void FLevelSimulation::persist_finalized_telemetry_run() {
    if (GIsAutomationTesting) {
        return;
    }

    auto record{level_telemetry_manager_.take_finalized_run()};
    if (!record.IsSet()) {
        return;
    }

    auto const path{write_level_telemetry_run(*record, level_telemetry_runs_directory())};
    if (path) {
        UE_LOG(LogSandbox, Display, TEXT("Wrote level telemetry run to '%s'"), **path);
        if (on_telemetry_persisted) {
            on_telemetry_persisted(record->metadata.run_id, {});
        }
    } else {
        UE_LOG(LogSandbox, Error, TEXT("Failed to write level telemetry run: %s"), *path.error());
        if (on_telemetry_persisted) {
            on_telemetry_persisted(record->metadata.run_id, path.error());
        }
    }
}

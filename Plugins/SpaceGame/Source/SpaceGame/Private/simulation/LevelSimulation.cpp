#include <HAL/PlatformMisc.h>
#include <SpaceGame/simulation/LevelSimulation.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace {
auto make_legacy_level_events(FLevelSimulationInitData const& data) -> ml::FCompiledLevelEvents {
    ml::FCompiledLevelEvents compiled;
    auto& initialisation{compiled.initialisation};
    auto& schedule{compiled.schedule};
    auto const player_offset{data.player.IsSet() ? 1 : 0};
    auto const capital_count{data.capital_spawns.num()};
    auto const turret_count{data.turret_spawns.num()};
    initialisation.entity_count = player_offset + capital_count + turret_count;
    initialisation.player_entity_index = data.player.IsSet() ? 0 : INDEX_NONE;

    schedule.capital_spawns.add_uninitialised(capital_count);
    for (int32 i{}; i < capital_count; ++i) {
        auto const entity_index{player_offset + i};
        schedule.capital_spawns.entity_indices[i] = entity_index;
        auto const target_index{data.capital_target_spawn_indices.IsValidIndex(i)
                                    ? data.capital_target_spawn_indices[i]
                                    : INDEX_NONE};
        schedule.capital_spawns.target_entity_indices[i] =
            target_index == FLevelSimulationInitData::player_target_spawn_index
                ? initialisation.player_entity_index
                : (target_index == INDEX_NONE ? INDEX_NONE : player_offset + target_index);
        ml::assign_from(schedule.capital_spawns.locations, i, data.capital_spawns.locations, i);
        ml::assign(schedule.capital_spawns.rotations,
                   i,
                   ml::get_rotator3d(data.capital_spawns.rotations, i));
        schedule.capital_spawns.teams[i] = data.capital_spawns.teams[i];
        schedule.capital_spawns.healths[i] = data.capital_spawns.healths[i];
        schedule.capital_spawns.initial_fighter_spawn_delays[i] =
            data.capital_spawns.initial_spawn_delays[i];
        schedule.capital_spawns.fighter_spawn_cooldowns[i] = data.capital_spawns.spawn_cooldowns[i];
    }

    schedule.turret_spawns.add_uninitialised(turret_count);
    for (int32 i{}; i < turret_count; ++i) {
        auto const entity_index{player_offset + capital_count + i};
        schedule.turret_spawns.entity_indices[i] = entity_index;
        ml::assign_from(schedule.turret_spawns.locations, i, data.turret_spawns.locations, i);
        auto const rotation{data.turret_transforms.IsValidIndex(i)
                                ? data.turret_transforms[i].Rotator()
                                : FRotator::ZeroRotator};
        ml::assign(schedule.turret_spawns.rotations, i, rotation);
        schedule.turret_spawns.teams[i] = data.turret_spawns.teams[i];
        schedule.turret_spawns.healths[i] = data.turret_spawns.healths[i];
        schedule.turret_spawns.laser_damages[i] = data.turret_spawns.laser_damages[i];
    }
    if (capital_count > 0 || turret_count > 0) {
        schedule.execution_ticks.Add(0);
        schedule.event_group_counts.AddDefaulted();
        schedule.add_spawn_group(ETestEntityType::CapitalShip, 0, capital_count);
        schedule.add_spawn_group(ETestEntityType::Turret, 0, turret_count);
    }
    return compiled;
}
}

FLevelSimulation::FLevelSimulation(FLevelSimulationInitData data,
                                   FLevelPresentationResources const* presentation) {
    clock_.initialise(data.clock_settings);
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
        data.level_events = make_legacy_level_events(data);
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
    level_telemetry_manager_.initialise(
        entity_registry_,
        {.active_count = lasers_simulation_.get_num_instances(),
         .cumulative_spawn_count = lasers_simulation_.get_number_spawned()});
    event_manager_.configure_mission();
    mission_manager_.begin_play();
    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::start() {
    check(state_ == EOrchestratorState::Paused);
    state_ = EOrchestratorState::Running;
}
void FLevelSimulation::pause() {
    check(state_ != EOrchestratorState::Uninitialised);
    state_ = EOrchestratorState::Paused;
}
void FLevelSimulation::set_time_scale(time_type scale) {
    check(scale > 0.0);
    clock_.tick_loop.time_scale = scale;
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

    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
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

            if (event_manager_.dispatch_tick(clock_.completed_ticks + 1)) {
                query_manager_.update();
            }
        }

        /* -------------------------------------------------------------------------------- */
        // Actor decision phase
        /* -------------------------------------------------------------------------------- */
        // Query target data from registry
        // Queue projectile spawns

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::update_timers);

            if (player_simulation_is_active()) {
                player_ship_phase_.update_timers(clock_.tick_loop.tick_period);
            }
            capital_ship_fighters_phase_.update_timers(clock_.tick_loop.tick_period);
            capital_ships_phase_.update_timers(clock_.tick_loop.tick_period);
            turrets_phase_.update_timers(clock_.tick_loop.tick_period);
            spinners_phase_.update_timers(clock_.tick_loop.tick_period);
        }

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::make_decisions);
            turrets_phase_.make_decisions();
            capital_ships_phase_.make_decisions();
            capital_ship_fighters_phase_.make_decisions();
        }

        /* -------------------------------------------------------------------------------- */
        // Simulation phase
        /* -------------------------------------------------------------------------------- */
        {
            // Movement
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::movement);

            if (player_simulation_is_active()) {
                player_ship_phase_.move(clock_.tick_loop.tick_period);
            }

            capital_ship_fighters_phase_.move(clock_.tick_loop.tick_period);
            spinners_phase_.move(clock_.tick_loop.tick_period);
        }

        {
            // Queue commands
            // e.g. spawning lasers for the next frame
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::queue_commands);

            if (player_simulation_is_active()) {
                player_ship_phase_.queue_commands();
            }

            capital_ship_fighters_phase_.queue_commands();
            turrets_phase_.queue_commands();
            spinners_phase_.queue_commands();
        }

        {
            // Projectile simulation
            TRACE_CPUPROFILER_EVENT_SCOPE(
                Sandbox::FLevelSimulation::advance::projectile_simulation);

            lasers_phase_.simulate(clock_.tick_loop.tick_period);
            lasers_phase_.commit_spawns();
        }

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

            capital_ships_phase_.resolve_damage_events();
            capital_ship_fighters_phase_.resolve_damage_events();
            turrets_phase_.resolve_damage_events();
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

            entity_registry_.commit_updates();
        }

        {
            // Apply changes from the registry e.g. destroyed targets
            TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FLevelSimulation::advance::sync_from_registry);

            capital_ships_phase_.sync_from_registry();
            capital_ship_fighters_phase_.sync_from_registry();
            turrets_phase_.sync_from_registry();
        }

        mission_manager_.mission_tick();
        if (on_mission_evaluated) {
            on_mission_evaluated();
        }

        if (presentation_.IsSet()) {
            presentation_->update_visual_data(clock_.tick_loop.tick_period);
        }

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
            entity_registry_.end_tick();
            query_manager_.update();
        }

        ++clock_.completed_ticks;
        level_telemetry_manager_.tick(
            clock_.completed_ticks,
            entity_registry_,
            {.active_count = lasers_simulation_.get_num_instances(),
             .cumulative_spawn_count = lasers_simulation_.get_number_spawned()});
        if (on_end_tick) {
            on_end_tick(*this);
        }
    }
}

void FLevelSimulation::commit_presentation(time_type const dt) {
    if (presentation_.IsSet()) {
        presentation_->commit_visual_data(dt);
    }
}

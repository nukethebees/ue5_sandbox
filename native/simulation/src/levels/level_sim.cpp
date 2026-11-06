#include <ioj/sim/levels/level_sim.h>

#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/lasers/phase_interface.h>
#include <ioj/sim/levels/spawn_context.h>
#include <ioj/sim/player/phase_interface.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spinners/phase_interface.h>
#include <ioj/sim/turrets/phase_interface.h>

#include <algorithm>
#include <cassert>
#include <optional>
#include <span>
#include <utility>

namespace ioj::sim {

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(LevelSimInitData data, std::pmr::memory_resource& memory)
    : memory_manager_{memory,
                      data.tick_memory_capacity_bytes,
                      data.phase_memory_capacity_bytes,
                      data.telemetry_recording.enabled
                          ? LevelMemoryManager::default_level_memory_capacity_bytes
                          : 0}
    , collision_pipeline_{entity_tables_, std::move(data.collision), &memory_manager_.game_memory()}
    , fighters_simulation_{entity_tables_}
    , capital_ships_simulation_{entity_tables_}
    , turrets_simulation_{entity_tables_}
    , spinners_simulation_{entity_tables_}
    , mission_{std::move(data.mission)}
    , expressions_{std::move(data.expressions)}
    , level_metadata_{std::move(data.level_metadata)}
    , event_manager_{std::move(data.level_events), memory_manager_.level_memory()}
    , spawn_manager_{capital_ships_simulation_,
                     fighters_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     data.capital_ships.fighter_spawn_slots_relative_transforms}
    , level_telemetry_manager_{clock_,
                               entity_ledger_,
                               lasers_simulation_,
                               memory_manager_.level_memory(),
                               data.telemetry_recording} {
    clock_.initialise(data.clock_settings);
    set_telemetry_metadata(std::move(data.telemetry_metadata));

    initialise_subsystems(data);

    auto const& events{event_manager_.data()};
    auto const& schedule{events.schedule};
    spawn_manager_.initialise(events.initialisation.entity_count,
                              schedule.capital_spawns.const_view(),
                              schedule.turret_spawns.const_view(),
                              schedule.spinner_spawns.const_view());
    if (events.initialisation.player_entity_index != invalid_level_entity_index) {
        spawn_manager_.set_entity_id(events.initialisation.player_entity_index,
                                     player_ship_simulation_->unique_entity_id);
    }
    spawn_manager_.spawn(schedule.initial_spawn_groups.const_view(), {entity_ledger_, clock_});
}
void LevelSim::finish_initialisation() {
    assert(state_ == LevelState::Uninitialised);

    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships::PhaseInterface{capital_ships_simulation_}.update_entity_lookup_table();
    fighters::PhaseInterface{fighters_simulation_}.update_entity_lookup_table();
    turrets::PhaseInterface{turrets_simulation_}.update_entity_lookup_table();
    spinners::PhaseInterface{spinners_simulation_}.update_entity_lookup_table();
    refresh_spatial_index();

    state_ = LevelState::StableSetup;
    clock_.advance_to(SimulationPhase::Thinking);

    level_telemetry_manager_.initialise();
    mission_.reset_progress();
    expressions_.reset_progress();
    event_manager_.activate(expressions_);
    task_manager_.initialise(mission_, expressions_);

    clock_.reset();
    state_ = LevelState::Paused;
}
void LevelSim::start() {
    assert(state_ == LevelState::Paused);
    state_ = LevelState::Running;
}
void LevelSim::pause() {
    assert(state_ == LevelState::Paused || state_ == LevelState::Running);
    state_ = LevelState::Paused;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void LevelSim::initialise_subsystems(LevelSimInitData const& data) {
    if (data.player) {
        auto& simulation{player_ship_simulation_.emplace(clock_, entity_ledger_, entity_tables_)};
        player::PhaseInterface{simulation}.initialise(*data.player);
    }

    assert(static_cast<std::size_t>(data.capital_ships.fighter_spawn_slots) ==
           data.capital_ships.fighter_spawn_slots_relative_transforms.size());
    capital_ships::PhaseInterface{capital_ships_simulation_}.initialise(
        data.capital_ships.max_health);
    auto const player_team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                                   : std::nullopt};
    participating_teams_ = event_manager_.data().schedule.get_participating_teams(player_team);
    assert(std::in_range<std::int32_t>(participating_teams_.num()));
    auto const participant_count{static_cast<std::int32_t>(participating_teams_.num())};
    fighter_per_team_limit_ =
        participant_count == 0 ? 0 : data.fighters.max_live_fighters.get() / participant_count;
    fighters::PhaseInterface{fighters_simulation_}.initialise(data.fighters, clock_);
    turrets::PhaseInterface{turrets_simulation_}.initialise(data.turrets, clock_);
    spinners::PhaseInterface{spinners_simulation_}.initialise(data.spinners, clock_);
    lasers::PhaseInterface{lasers_simulation_}.initialise(data.lasers);
}

/* **************************************** */
// Configuration and commands
/* **************************************** */
void LevelSim::set_telemetry_metadata(telemetry::RunMetadata metadata) {
    level_telemetry_manager_.set_run_metadata(std::move(metadata));
}
void LevelSim::set_static_collision(std::span<collision::WorldAABB const> bounds) {
    assert(state_ == LevelState::Uninitialised);
    collision_pipeline_.set_static_collision(bounds);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void LevelSim::finalize_telemetry_run(telemetry::RunEndReason const reason) {
    level_telemetry_manager_.finalize(reason);
}
void LevelSim::stop_run(telemetry::RunEndReason const reason) {
    level_telemetry_manager_.finalize(reason);
    state_ = LevelState::Paused;
}
auto LevelSim::player_team_completion() const -> std::optional<TeamMissionCompletion> {
    auto const team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                            : std::nullopt};
    return mission_.team_completion(team);
}

auto LevelSim::take_finalized_telemetry_run() -> std::optional<telemetry::RunRecord> {
    return level_telemetry_manager_.take_finalized_run(clock_.tick_rate());
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt, fighters::NavigationCounters* const navigation_output) {
    SANDBOX_PROFILE_SCOPE("LevelSim::advance");
    if (state_ != LevelState::Running) {
        return;
    }

    entity_ledger_.reset_frame_output();
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    collision_pipeline_.reset_frame_output();
    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_loop.get_tick_period())};
        if (!player_ship_simulation_) {
            advance_tick<TickPlayerShipState::NotPresent>(tick_period, navigation_output);
        } else if (player_ship_simulation_->is_alive()) {
            advance_tick<TickPlayerShipState::PresentAlive>(tick_period, navigation_output);
        } else {
            advance_tick<TickPlayerShipState::PresentDead>(tick_period, navigation_output);
        }

        profiling::mark_frame("Simulation");
        clock_.transition_to(SimulationPhase::BetweenTicks);
        dispatch_terminal_events();
        if (state_ != LevelState::Running) {
            break;
        }
    }
}

void LevelSim::dispatch_terminal_events() {
    auto const events{event_manager_.ready_events()};
    std::ranges::sort(events);
    if (events.empty()) {
        return;
    }

    auto const first{events.front()};
    switch (first.kind()) {
        case EventKind::MissionCompleted:
            stop_run(telemetry::RunEndReason::MissionCompleted);
            break;
        case EventKind::StopRun:
            stop_run(event_manager_.data().schedule.stop_events[first.index()]);
            break;
        case EventKind::Spawn:
            return;
    }
    event_manager_.clear_ready();
}

/* **************************************** */
// Tick phases
/* **************************************** */
template <TickPlayerShipState PlayerState>
void LevelSim::advance_tick(float const tick_period,
                            fighters::NavigationCounters* const navigation_output) {
    prepare_tick<PlayerState>(tick_period);
    {
        std::pmr::vector<lasers::LaserIndex> laser_removal_indices{&memory_manager_.tick_memory()};
        {
            FrameArray<std::uint32_t> fighter_fire_indices{&memory_manager_.tick_memory()};
            FrameArray<std::uint32_t> spinner_fire_indices{&memory_manager_.tick_memory()};
            think_tick<PlayerState>(
                tick_period, fighter_fire_indices, spinner_fire_indices, navigation_output);
            act_tick<PlayerState>(
                tick_period, fighter_fire_indices, spinner_fire_indices, laser_removal_indices);
        }
        resolve_tick<PlayerState>(laser_removal_indices);
    }
    memory_manager_.tick_memory().reset();
    memory_manager_.phase_memory().reset();
}
template <TickPlayerShipState PlayerState>
void LevelSim::prepare_tick(float const tick_period) {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};

    SANDBOX_PROFILE_SCOPE("Preparation");
    clock_.transition_to(SimulationPhase::Preparation);

    auto const previous_issued_counts{entity_ledger_.issued_counts()};
    auto const& schedule{event_manager_.data().schedule};
    spawn_manager_.process_events(event_manager_.ready_events(),
                                  schedule.spawn_events,
                                  schedule.spawn_groups,
                                  {entity_ledger_, clock_});
    event_manager_.clear_ready();

    fighters::PreparationInputs const fighter_inputs{.ledger = entity_ledger_,
                                                     .participating_teams = participating_teams_,
                                                     .per_team_limit = fighter_per_team_limit_};
    fighters_phase.commit_spawns(fighter_inputs);

    lasers_phase.commit_spawns({clock_, combat_events_});

    // Borrow fresh owner views after all spawn commits.
    collision_pipeline_.collect_spawn_candidates(
        previous_issued_counts,
        capital_ships_simulation_.get_entities().entity_ids(),
        fighters_simulation_.entities().entity_ids(),
        turrets_simulation_.get_entities().entity_ids());

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.prepare_tick(tick_period);
    } else if constexpr (PlayerState == TickPlayerShipState::PresentDead) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_phase.prepare_tick();
    fighters_phase.prepare_tick(fighter_inputs);
    turrets_phase.prepare_tick();
    spinners_phase.prepare_tick(tick_period);

    refresh_spatial_index();
}
template <TickPlayerShipState PlayerState>
void LevelSim::think_tick(float const tick_period,
                          FrameArray<std::uint32_t>& fighter_fire_indices,
                          FrameArray<std::uint32_t>& spinner_fire_indices,
                          fighters::NavigationCounters* const navigation_output) {
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};

    auto& phase_memory{memory_manager_.phase_memory()};
    FrameScratchScope scratch_scope{phase_memory};

    SANDBOX_PROFILE_SCOPE("Thinking");
    clock_.transition_to(SimulationPhase::Thinking);

    task_manager_.plan_tasks(
        {.expressions = expressions_, .entities = spawn_manager_.entity_ids()});

    spawn_manager_.queue_fighter_spawns(tick_period, phase_memory, participating_teams_);

    turrets::ThinkingInputs const turret_inputs{
        collision_pipeline_.queries(), lasers_simulation_, phase_memory};
    fighters::NavigationCounters navigation_counters{};
    fighters::ThinkingInputs const fighter_inputs{
        .clock = clock_,
        .task_manager = task_manager_,
        .collision_radius = collision_pipeline_.queries().entity_type_radius(EntityType::Fighter),
        .spatial_query_manager = collision_pipeline_.queries(),
        .scratch_resource = phase_memory,
        .pending_fire_indices = fighter_fire_indices,
        .navigation_counters = navigation_counters};

    turrets_phase.think(turret_inputs);
    fighters_phase.think(fighter_inputs);
    if (navigation_output) {
        *navigation_output = navigation_counters;
    }
    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.think(tick_period);
    }
    spinners_phase.think(tick_period);

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.generate_fire_commands();
    }
    fighters_phase.generate_fire_commands(fighter_inputs);
    turrets_phase.generate_fire_commands(turret_inputs);
    spinners_phase.generate_fire_commands({spinner_fire_indices});
}
template <TickPlayerShipState PlayerState>
void LevelSim::act_tick(float const tick_period,
                        std::span<std::uint32_t const> const fighter_fire_indices,
                        std::span<std::uint32_t const> const spinner_fire_indices,
                        std::pmr::vector<lasers::LaserIndex>& laser_removal_indices) {
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};
    auto& phase_memory{memory_manager_.phase_memory()};
    FrameScratchScope scratch_scope{phase_memory};
    FrameArray<EntityId> fighter_overlap_candidates{&phase_memory};

    SANDBOX_PROFILE_SCOPE("Action");
    clock_.transition_to(SimulationPhase::Action);

    {
        lasers::ActionInputs const inputs{tick_period,
                                          combat_events_,
                                          clock_.get_completed_ticks(),
                                          collision_pipeline_.queries(),
                                          phase_memory,
                                          laser_removal_indices};
        lasers_phase.simulate(inputs);
    }

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        auto const before{player_ship_simulation_->physical_state().transform};
        player::ActionInputs const inputs{lasers_simulation_};
        player::PhaseInterface{*player_ship_simulation_}.apply_movement(inputs);
        collision_pipeline_.track_player_movement(
            player_ship_simulation_->unique_entity_id,
            before,
            player_ship_simulation_->physical_state().transform);
    }

    {
        fighters::ActionInputs const fighter_inputs{tick_period,
                                                    lasers_simulation_,
                                                    phase_memory,
                                                    fighter_fire_indices,
                                                    fighter_overlap_candidates};
        spinners::ActionInputs const spinner_inputs{lasers_simulation_, spinner_fire_indices};
        fighters_phase.apply_movement(fighter_inputs);
        spinners_phase.apply_movement(spinner_inputs);
    }

    collision_pipeline_.detect_and_handle_overlaps(
        read_access(), fighter_overlap_candidates, combat_events_, phase_memory);
}
template <TickPlayerShipState PlayerState>
void LevelSim::resolve_tick(std::pmr::vector<lasers::LaserIndex>& laser_removal_indices) {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};
    auto& phase_memory{memory_manager_.phase_memory()};
    std::pmr::vector<std::uint32_t> fighter_removal_indices{&memory_manager_.tick_memory()};
    std::pmr::vector<std::uint32_t> capital_removal_indices{&memory_manager_.tick_memory()};
    std::pmr::vector<std::uint32_t> turret_removal_indices{&memory_manager_.tick_memory()};

    SANDBOX_PROFILE_SCOPE("Resolution");
    clock_.transition_to(SimulationPhase::Resolution);

    {
        FrameScratchScope scratch_scope{phase_memory};
        EntityDeathInfo fighter_deaths{phase_memory};
        EntityDeathInfo capital_deaths{phase_memory};
        EntityDeathInfo turret_deaths{phase_memory};

        combat_events_.prepare(entity_tables_.lookups, &phase_memory);

        capital_ships::ResolutionInputs const capital_inputs{combat_events_,
                                                             entity_ledger_,
                                                             clock_.get_completed_ticks(),
                                                             phase_memory,
                                                             capital_removal_indices,
                                                             capital_deaths};
        fighters::ResolutionInputs const fighter_inputs{.combat_events = combat_events_,
                                                        .ledger = entity_ledger_,
                                                        .tick = clock_.get_completed_ticks(),
                                                        .scratch_resource = phase_memory,
                                                        .local_indices_to_remove =
                                                            fighter_removal_indices,
                                                        .entity_death_info = fighter_deaths};
        turrets::ResolutionInputs const turret_inputs{combat_events_,
                                                      entity_ledger_,
                                                      clock_.get_completed_ticks(),
                                                      phase_memory,
                                                      turret_removal_indices,
                                                      turret_deaths};

        if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
            player::ResolutionInputs const inputs{combat_events_, phase_memory};
            player::PhaseInterface{*player_ship_simulation_}.resolve_damage_events(inputs);
        }
        capital_ships_phase.resolve_damage_events(capital_inputs);
        fighters_phase.resolve_damage_events(fighter_inputs);
        turrets_phase.resolve_damage_events(turret_inputs);

        {
            SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
            capital_ships_phase.publish_deaths(capital_inputs);
            fighters_phase.publish_deaths(fighter_inputs);
            turrets_phase.publish_deaths(turret_inputs);
        }

        // Evaluate final health and deaths before invalidating row mappings.
        SimTick const tick{clock_.completed_ticks.value() + 1};
        expressions_.evaluate({tick,
                               entity_ledger_,
                               spawn_manager_.entity_ids(),
                               &collision_pipeline_.queries(),
                               &phase_memory},
                              mission_);
        auto const completion_count{mission_.completions().size()};
        auto const changes{
            mission_.evaluate({tick, &phase_memory, participating_teams_}, expressions_)};
        if (mission_.completions().size() != completion_count) {
            event_manager_.enqueue(EventHandle{0, EventKind::MissionCompleted});
        }
        auto const& objectives{mission_.objectives()};
        for (auto const id : changes.completed) {
            expressions_.deactivate(objectives.leaves(id), objectives.composites(id));
        }
        for (auto const id : changes.activated) {
            expressions_.activate(objectives.leaves(id), objectives.composites(id), tick);
        }
        event_manager_.collect_ready(expressions_);
    }

    {
        FrameScratchScope scratch_scope{phase_memory};

        SANDBOX_PROFILE_SCOPE("ResolutionCommit");
        clock_.transition_to(SimulationPhase::ResolutionCommit);
        fighters::ResolutionCommitInputs const fighter_inputs{fighter_removal_indices};
        capital_ships::ResolutionCommitInputs const capital_inputs{capital_removal_indices};
        turrets::ResolutionCommitInputs const turret_inputs{turret_removal_indices};

        // Sort removals before retiring and compacting entity rows.
        capital_ships_phase.remove_components(capital_inputs);
        fighters_phase.remove_components(fighter_inputs);
        turrets_phase.remove_components(turret_inputs);

        capital_ships_phase.remove_entities(capital_inputs);
        fighters_phase.remove_entities(fighter_inputs);
        turrets_phase.remove_entities(turret_inputs);
        lasers_phase.cleanup_entities({laser_removal_indices, clock_});
    }

    capital_ships_phase.finish_action();
    fighters_phase.finish_action();
    turrets_phase.finish_action();
    lasers_phase.finish_action();
    clock_.completed_ticks.set_value(clock_.completed_ticks.value() + 1);
    level_telemetry_manager_.tick();
    combat_events_.reset();
}

/* **************************************** */
// Read views
/* **************************************** */

void LevelSim::refresh_spatial_index() {
    collision_pipeline_.refresh_spatial_index(read_access());
}
} // namespace ioj::sim

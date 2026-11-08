#include <ioj/sim/levels/level_sim.h>

#include <ioj/diagnostics.h>
#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/lasers/phase_interface.h>
#include <ioj/sim/levels/spawn_context.h>
#include <ioj/sim/player/phase_interface.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spinners/phase_interface.h>
#include <ioj/sim/turrets/phase_interface.h>

#include <cassert>
#include <optional>
#include <span>
#include <utility>

namespace ioj::sim {

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(LevelSimInitData data, std::pmr::memory_resource& memory)
    : memory_manager_{
          memory,
          data.tick_memory_capacity_bytes,
          data.phase_memory_capacity_bytes,
          data.telemetry_recording.enabled ? LevelMemoryManager::default_level_memory_capacity_bytes
                                          : 0,
      }
    , collision_pipeline_{entity_tables_, std::move(data.collision), &memory_manager_.game_memory()}
    , fighters_simulation_{entity_tables_}
    , capital_ships_simulation_{entity_tables_}
    , turrets_simulation_{entity_tables_}
    , spinners_simulation_{entity_tables_}
    , latches_{std::move(data.latches)}
    , expressions_{std::move(data.expressions)}
    , timers_{std::move(data.timers)}
    , level_metadata_{std::move(data.level_metadata)}
    , event_manager_{std::move(data.level_events), memory_manager_.level_memory()}
    , spawn_manager_{
          capital_ships_simulation_,
          fighters_simulation_,
          turrets_simulation_,
          spinners_simulation_,
          data.capital_ships.fighter_spawn_slots_relative_transforms,
      }
    , level_telemetry_manager_{
          clock_,
          entity_ledger_,
          lasers_simulation_,
          memory_manager_.level_memory(),
          data.telemetry_recording,
      } {
    clock_.initialise(data.clock_settings);
    set_telemetry_metadata(std::move(data.telemetry_metadata));

    initialise_subsystems(data);

    spawn_manager_.initialise(event_manager_.data().spawn_count() +
                              static_cast<std::uint32_t>(player_ship_simulation_.has_value()));
    if (data.player_entity_ref.is_valid()) {
        spawn_manager_.set_entity_id(data.player_entity_ref,
                                     player_ship_simulation_->unique_entity_id);
    }
}
void LevelSim::finish_initialisation() {
    assert(state_ == LevelState::Uninitialised);

    dispatch_preparation_events();

    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships::InitialisationInterface{capital_ships_simulation_}.update_entity_lookup_table();
    fighters::InitialisationInterface{fighters_simulation_}.update_entity_lookup_table();
    turrets::InitialisationInterface{turrets_simulation_}.update_entity_lookup_table();
    spinners::InitialisationInterface{spinners_simulation_}.update_entity_lookup_table();
    refresh_spatial_index();

    state_ = LevelState::StableSetup;
    clock_.advance_to(SimulationPhase::Thinking);

    level_telemetry_manager_.initialise();

    clock_.reset();
    state_ = LevelState::Paused;
    dispatch_tick_end_events();
}
void LevelSim::start() {
    assert(state_ == LevelState::Paused);
    state_ = LevelState::Running;
}
void LevelSim::pause() {
    assert(state_ == LevelState::Paused || state_ == LevelState::Running);
    event_manager_.enqueue(EventHandle{0, EventKind::Pause});
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void LevelSim::initialise_subsystems(LevelSimInitData const& data) {
    if (data.player) {
        auto& simulation{player_ship_simulation_.emplace(clock_, entity_ledger_, entity_tables_)};
        player::InitialisationInterface{simulation}.initialise(*data.player);
    }

    assert(static_cast<std::size_t>(data.capital_ships.fighter_spawn_slots) ==
           data.capital_ships.fighter_spawn_slots_relative_transforms.size());
    capital_ships::InitialisationInterface{capital_ships_simulation_}.initialise(
        data.capital_ships.max_health);
    auto const player_team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                                   : std::nullopt};
    participating_teams_ = event_manager_.data().collect_participating_teams(player_team);
    assert(std::in_range<std::int32_t>(participating_teams_.num()));
    auto const participant_count{static_cast<std::int32_t>(participating_teams_.num())};
    fighter_per_team_limit_ =
        participant_count == 0 ? 0 : data.fighters.max_live_fighters.get() / participant_count;
    fighters::InitialisationInterface{fighters_simulation_}.initialise(data.fighters, clock_);
    turrets::InitialisationInterface{turrets_simulation_}.initialise(data.turrets, clock_);
    spinners::InitialisationInterface{spinners_simulation_}.initialise(data.spinners, clock_);
    lasers::InitialisationInterface{lasers_simulation_}.initialise(data.lasers);
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
// Telemetry and level results
/* **************************************** */
void LevelSim::finalize_telemetry_run(telemetry::RunEndReason const reason) {
    level_telemetry_manager_.finalize(reason);
}
void LevelSim::stop_run(telemetry::RunEndReason const reason) {
    if (state_ == LevelState::Stopped) {
        return;
    }
    event_manager_.enqueue_stop(reason);
    dispatch_tick_end_events();
}
auto LevelSim::result() const -> std::optional<LevelResult> {
    if (!terminal_) {
        return std::nullopt;
    }
    auto const& definitions{event_manager_.data()};
    auto const event{terminal_->event};
    if (event.kind() == EventKind::Win) {
        return LevelResult{*terminal_,
                           definitions.win_events[event.index()],
                           telemetry::RunEndReason::LevelCompleted};
    }
    return LevelResult{*terminal_, std::nullopt, definitions.stop_events[event.index()]};
}

auto LevelSim::take_finalized_telemetry_run() -> std::optional<telemetry::RunRecord> {
    return level_telemetry_manager_.take_finalized_run(clock_.tick_rate());
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt, fighters::NavigationCounters* const navigation_output) {
    SANDBOX_PROFILE_SCOPE("LevelSim::advance");
    if (state_ == LevelState::Stopped) {
        return;
    }

    dispatch_tick_end_events();
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
        dispatch_tick_end_events();
        if (state_ != LevelState::Running) {
            break;
        }
    }
}

void LevelSim::dispatch_tick_end_events() {
    auto const& definitions{event_manager_.data()};
    if (state_ == LevelState::Stopped) {
        return;
    }
    while (auto const event = event_manager_.consume_tick_end_event()) {
        switch (event->kind()) {
            case EventKind::StartTimer: {
                auto const& start{definitions.start_timer_events[event->index()]};
                timers_.start(start.timer, start.duration);
                break;
            }
            case EventKind::PublishSignal:
                event_manager_.publish(definitions.publish_events[event->index()]);
                break;
            case EventKind::Win:
            case EventKind::StopRun:
                terminal_ = TerminalEvent{*event, clock_.get_completed_ticks()};
                level_telemetry_manager_.finalize(result()->reason);
                state_ = LevelState::Stopped;
                return;
            case EventKind::Pause:
                state_ = LevelState::Paused;
                break;
            default:
                fatal_error("Invalid tick-end event kind");
        }
    }
}
void LevelSim::dispatch_preparation_events() {
    while (auto const event = event_manager_.consume_preparation_event()) {
        switch (event->kind()) {
            case EventKind::Spawn:
                spawn_manager_.process_event(*event, event_manager_, {entity_ledger_, clock_});
                break;
            default:
                fatal_error("Invalid Preparation event kind");
        }
    }
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
    capital_ships::PreparationInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PreparationInterface fighters_phase{fighters_simulation_};
    turrets::PreparationInterface turrets_phase{turrets_simulation_};
    spinners::PreparationInterface spinners_phase{spinners_simulation_};
    lasers::PreparationInterface lasers_phase{lasers_simulation_};

    SANDBOX_PROFILE_SCOPE("Preparation");
    clock_.transition_to(SimulationPhase::Preparation);

    auto const previous_issued_counts{entity_ledger_.issued_counts()};
    dispatch_preparation_events();

    fighters::PreparationInputs const fighter_inputs{
        .ledger = entity_ledger_,
        .participating_teams = participating_teams_,
        .per_team_limit = fighter_per_team_limit_,
    };
    fighters_phase.commit_spawns(fighter_inputs);

    lasers_phase.commit_spawns({clock_, combat_events_});

    // Borrow fresh owner views after all spawn commits.
    collision_pipeline_.collect_spawn_candidates(
        previous_issued_counts,
        capital_ships_simulation_.get_entities().entity_ids(),
        fighters_simulation_.entities().entity_ids(),
        turrets_simulation_.get_entities().entity_ids());

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PreparationInterface{*player_ship_simulation_}.prepare_tick(tick_period);
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
    fighters::ThinkingInterface fighters_phase{fighters_simulation_};
    turrets::ThinkingInterface turrets_phase{turrets_simulation_};
    spinners::ThinkingInterface spinners_phase{spinners_simulation_};

    auto& phase_memory{memory_manager_.phase_memory()};
    FrameScratchScope scratch_scope{phase_memory};

    SANDBOX_PROFILE_SCOPE("Thinking");
    clock_.transition_to(SimulationPhase::Thinking);

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
        .navigation_counters = navigation_counters,
    };

    turrets_phase.think(turret_inputs);
    fighters_phase.think(fighter_inputs);
    if (navigation_output) {
        *navigation_output = navigation_counters;
    }
    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::ThinkingInterface{*player_ship_simulation_}.think(tick_period);
    }
    spinners_phase.think(tick_period);

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::ThinkingInterface{*player_ship_simulation_}.generate_fire_commands();
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
    fighters::ActionInterface fighters_phase{fighters_simulation_};
    spinners::ActionInterface spinners_phase{spinners_simulation_};
    lasers::ActionInterface lasers_phase{lasers_simulation_};
    auto& phase_memory{memory_manager_.phase_memory()};
    FrameScratchScope scratch_scope{phase_memory};
    FrameArray<EntityId> fighter_overlap_candidates{&phase_memory};

    SANDBOX_PROFILE_SCOPE("Action");
    clock_.transition_to(SimulationPhase::Action);

    {
        lasers::ActionInputs const inputs{
            tick_period,
            combat_events_,
            clock_.get_completed_ticks(),
            collision_pipeline_.queries(),
            phase_memory,
            laser_removal_indices,
        };
        lasers_phase.simulate(inputs);
    }

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        auto const before{player_ship_simulation_->physical_state().transform};
        player::ActionInputs const inputs{lasers_simulation_};
        player::ActionInterface{*player_ship_simulation_}.apply_movement(inputs);
        collision_pipeline_.track_player_movement(
            player_ship_simulation_->unique_entity_id,
            before,
            player_ship_simulation_->physical_state().transform);
    }

    {
        fighters::ActionInputs const fighter_inputs{
            tick_period,
            lasers_simulation_,
            phase_memory,
            fighter_fire_indices,
            fighter_overlap_candidates,
        };
        spinners::ActionInputs const spinner_inputs{lasers_simulation_, spinner_fire_indices};
        fighters_phase.apply_movement(fighter_inputs);
        spinners_phase.apply_movement(spinner_inputs);
    }

    collision_pipeline_.detect_and_handle_overlaps(
        read_access(), fighter_overlap_candidates, combat_events_, phase_memory);
}
template <TickPlayerShipState PlayerState>
void LevelSim::resolve_tick(std::pmr::vector<lasers::LaserIndex>& laser_removal_indices) {
    capital_ships::ResolutionInterface capital_ships_phase{capital_ships_simulation_};
    fighters::ResolutionInterface fighters_phase{fighters_simulation_};
    turrets::ResolutionInterface turrets_phase{turrets_simulation_};
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

        capital_ships::ResolutionInputs const capital_inputs{
            combat_events_,
            entity_ledger_,
            clock_.get_completed_ticks(),
            phase_memory,
            capital_removal_indices,
            capital_deaths,
        };
        fighters::ResolutionInputs const fighter_inputs{
            .combat_events = combat_events_,
            .ledger = entity_ledger_,
            .tick = clock_.get_completed_ticks(),
            .scratch_resource = phase_memory,
            .local_indices_to_remove = fighter_removal_indices,
            .entity_death_info = fighter_deaths,
        };
        turrets::ResolutionInputs const turret_inputs{
            combat_events_,
            entity_ledger_,
            clock_.get_completed_ticks(),
            phase_memory,
            turret_removal_indices,
            turret_deaths,
        };

        if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
            player::ResolutionInputs const inputs{combat_events_, phase_memory};
            player::ResolutionInterface{*player_ship_simulation_}.resolve_damage_events(inputs);
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
        latches_.commit();
        timers_.tick(SimTick{1});
        expressions_.evaluate({entity_ledger_,
                               timers_,
                               spawn_manager_.entity_ids(),
                               latches_,
                               event_manager_.signals(),
                               &collision_pipeline_.queries(),
                               &phase_memory});
        latches_.evaluate(expressions_);
        event_manager_.collect_ready(expressions_);
    }

    capital_ships::ResolutionCommitInterface capital_ships_commit{capital_ships_simulation_};
    fighters::ResolutionCommitInterface fighters_commit{fighters_simulation_};
    turrets::ResolutionCommitInterface turrets_commit{turrets_simulation_};
    lasers::ResolutionCommitInterface lasers_commit{lasers_simulation_};

    {
        FrameScratchScope scratch_scope{phase_memory};

        SANDBOX_PROFILE_SCOPE("ResolutionCommit");
        clock_.transition_to(SimulationPhase::ResolutionCommit);
        fighters::ResolutionCommitInputs const fighter_inputs{fighter_removal_indices};
        capital_ships::ResolutionCommitInputs const capital_inputs{capital_removal_indices};
        turrets::ResolutionCommitInputs const turret_inputs{turret_removal_indices};

        // Sort removals before retiring and compacting entity rows.
        capital_ships_commit.remove_components(capital_inputs);
        fighters_commit.remove_components(fighter_inputs);
        turrets_commit.remove_components(turret_inputs);

        capital_ships_commit.remove_entities(capital_inputs);
        fighters_commit.remove_entities(fighter_inputs);
        turrets_commit.remove_entities(turret_inputs);
        lasers_commit.cleanup_entities({laser_removal_indices, clock_});
    }

    capital_ships_commit.finish_action();
    fighters_commit.finish_action();
    turrets_commit.finish_action();
    lasers_commit.finish_action();
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

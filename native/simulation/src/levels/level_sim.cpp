#include <ioj/sim/levels/level_sim.h>

#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/lasers/phase_interface.h>
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
LevelSim::LevelSim(GameMemory& game_memory, LevelSimInitData data)
    : frame_memory_block_{game_memory.acquire_block(data.frame_memory_capacity_bytes,
                                                    FrameMemoryResource::backing_alignment)}
    , frame_memory_{std::span<std::byte>{frame_memory_block_.data(),
                                         frame_memory_block_.size_bytes()}}
    , collision_pipeline_{entity_tables_, std::move(data.collision), &game_memory.memory_resource()}
    , lasers_simulation_{clock_, combat_events_}
    , fighters_simulation_{clock_, entity_ledger_, entity_tables_}
    , capital_ships_simulation_{entity_ledger_, entity_tables_, fighters_simulation_}
    , turrets_simulation_{clock_, entity_ledger_, entity_tables_}
    , spinners_simulation_{clock_, entity_ledger_, entity_tables_}
    , mission_{std::move(data.mission)}
    , result_metadata_{std::move(data.result_metadata)}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     std::move(data.level_events)}
    , level_telemetry_manager_{
          clock_, entity_ledger_, lasers_simulation_, game_memory, data.telemetry_history} {
    clock_.initialise(data.clock_settings);
    set_telemetry_metadata(std::move(data.telemetry_metadata));

    initialise_subsystems(data);

    event_manager_.begin(player_ship_simulation_ ? player_ship_simulation_->unique_entity_id
                                                 : EntityId{});
    event_manager_.execute_tick(SimTick{});
}
void LevelSim::finish_initialisation() {
    assert(state_ == OrchestratorState::Uninitialised);

    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_simulation_.update_entity_lookup_table();
    fighters_simulation_.update_entity_lookup_table();
    turrets_simulation_.update_entity_lookup_table();
    spinners_simulation_.update_entity_lookup_table();
    refresh_spatial_index();

    clock_.transition_to(SimulationPhase::StableSetup);

    level_telemetry_manager_.initialise();
    {
        FrameScratchScope scratch_scope{frame_memory_};
        mission_.begin({SimTick{},
                        entity_ledger_,
                        event_manager_.get_entity_ids(),
                        &collision_pipeline_.queries(),
                        &frame_memory_});
    }

    state_ = OrchestratorState::Paused;
    clock_.transition_to(SimulationPhase::BetweenTicks);
}
void LevelSim::start() {
    assert(state_ == OrchestratorState::Paused);
    state_ = OrchestratorState::Running;
}
void LevelSim::pause() {
    assert(state_ != OrchestratorState::Uninitialised);
    state_ = OrchestratorState::Paused;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void LevelSim::initialise_subsystems(LevelSimInitData const& data) {
    if (data.player) {
        auto& simulation{player_ship_simulation_.emplace(clock_, entity_ledger_, entity_tables_)};
        player::PhaseInterface{simulation}.initialise(*data.player);
    }

    capital_ships::PhaseInterface{capital_ships_simulation_}.initialise(data.capital_ships);
    auto const player_team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                                   : std::nullopt};
    auto const teams{event_manager_.get_participating_teams(player_team)};
    fighters::PhaseInterface{fighters_simulation_}.initialise(
        data.fighters,
        {.participating_teams = teams,
         .collision_radius = PositiveFinite<float>::make(
             collision_pipeline_.queries().get_entity_type_radius(EntityType::Fighter))});
    turrets::PhaseInterface{turrets_simulation_}.initialise(data.turrets);
    spinners::PhaseInterface{spinners_simulation_}.initialise(data.spinners);
    lasers::PhaseInterface{lasers_simulation_}.initialise(data.lasers);
}

/* **************************************** */
// Configuration and commands
/* **************************************** */
void LevelSim::set_telemetry_metadata(LevelTelemetryRunMetadata metadata) {
    level_telemetry_manager_.set_run_metadata(std::move(metadata));
}
void LevelSim::set_static_collision(std::span<collision::WorldAABB const> bounds) {
    assert(state_ == OrchestratorState::Uninitialised);
    collision_pipeline_.set_static_collision(bounds);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void LevelSim::finalize_telemetry_run(LevelTelemetryRunEndReason const reason) {
    level_telemetry_manager_.finalize(reason);
}
void LevelSim::stop_run(LevelTelemetryRunEndReason const reason) {
    level_telemetry_manager_.finalize(reason);
    state_ = OrchestratorState::Paused;
}
void LevelSim::stop_run_if_requested(RunStopConditions const& conditions) {
    if (state_ != OrchestratorState::Running) {
        return;
    }

    auto const has_future_spawns{conditions.stop_when_battle_resolved &&
                                 event_manager_.has_future_spawns()};
    if (auto const reason{conditions.stop_reason(clock_, entity_ledger_, has_future_spawns)}) {
        stop_run(*reason);
    }
}
auto LevelSim::take_player_team_completion() -> std::optional<TeamCompletion> {
    auto const team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                            : std::nullopt};
    auto completion{mission_.take_team_completion(team)};
    if (completion) {
        level_telemetry_manager_.finalize_mission(completion->outcome);
    }
    return completion;
}

auto LevelSim::take_finalized_telemetry_run() -> std::optional<LevelTelemetryRunRecord> {
    return level_telemetry_manager_.take_finalized_run(clock_.get_tick_rate());
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt) {
    SANDBOX_PROFILE_SCOPE("LevelSim::advance");
    if (state_ != OrchestratorState::Running) {
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
        auto const player_active{player_ship_simulation_.has_value() &&
                                 player_ship_simulation_->is_alive()};

        prepare_tick(tick_period, player_active);
        think_tick(tick_period, player_active);
        act_tick(tick_period, player_active);
        resolve_tick(player_active);

        profiling::mark_frame("Simulation");
        clock_.transition_to(SimulationPhase::BetweenTicks);
        if (state_ != OrchestratorState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Tick phases
/* **************************************** */
void LevelSim::prepare_tick(float const tick_period, bool const player_active) {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};

    SANDBOX_PROFILE_SCOPE("Preparation");
    clock_.transition_to(SimulationPhase::Preparation);

    fighters_phase.commit_orders();
    auto const previous_issued_counts{entity_ledger_.get_issued_counts()};
    event_manager_.execute_tick(SimTick{clock_.completed_ticks.value() + 1});

    auto const previous_fighter_count{fighters_simulation_.get_num_instances()};
    fighters_phase.commit_spawns();
    capital_ships_simulation_.fighters_spawned +=
        fighters_simulation_.get_num_instances() - previous_fighter_count;

    lasers_phase.commit_spawns();

    // Borrow fresh owner views after all spawn commits.
    collision_pipeline_.collect_spawn_candidates(
        previous_issued_counts,
        capital_ships_simulation_.get_entities().entity_ids(),
        fighters_simulation_.get_entities().entity_ids(),
        turrets_simulation_.get_entities().entity_ids());

    if (player_active) {
        player::PhaseInterface{*player_ship_simulation_}.prepare_tick(tick_period);
    } else if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_phase.prepare_tick(tick_period);
    fighters_phase.prepare_tick(tick_period);
    turrets_phase.prepare_tick(tick_period);
    spinners_phase.prepare_tick(tick_period);

    refresh_spatial_index();
}
void LevelSim::think_tick(float const tick_period, bool const player_active) {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};

    FrameScratchScope scratch_scope{frame_memory_};
    auto* const scratch_resource{&frame_memory_};

    SANDBOX_PROFILE_SCOPE("Thinking");
    clock_.transition_to(SimulationPhase::Thinking);

    // Resolve dependent reads after every entity type has updated its lookup table.
    capital_ships_simulation_.refresh_fighter_ids(scratch_resource);

    turrets::ThinkingInputs const turret_inputs{
        collision_pipeline_.queries(), lasers_simulation_, frame_memory_};
    capital_ships::ThinkingInputs const capital_inputs{collision_pipeline_.queries(),
                                                       frame_memory_};
    fighters::ThinkingInputs const fighter_inputs{
        tick_period, collision_pipeline_.queries(), frame_memory_};

    turrets_phase.think(turret_inputs);
    capital_ships_phase.think(capital_inputs);
    fighters_phase.think(fighter_inputs);
    if (player_active) {
        player::PhaseInterface{*player_ship_simulation_}.think(tick_period);
    }
    spinners_phase.think(tick_period);

    if (player_active) {
        player::PhaseInterface{*player_ship_simulation_}.generate_fire_commands();
    }
    fighters_phase.generate_fire_commands(fighter_inputs);
    turrets_phase.generate_fire_commands(turret_inputs);
    spinners_phase.generate_fire_commands();
}
void LevelSim::act_tick(float const tick_period, bool const player_active) {
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};

    SANDBOX_PROFILE_SCOPE("Action");
    clock_.transition_to(SimulationPhase::Action);

    {
        FrameScratchScope scratch_scope{frame_memory_};
        lasers::ActionInputs const inputs{
            tick_period, collision_pipeline_.queries(), frame_memory_};
        lasers_phase.simulate(inputs);
    }

    if (player_active) {
        auto const before{player_ship_simulation_->get_physical_state().transform};
        player::ActionInputs const inputs{lasers_simulation_};
        player::PhaseInterface{*player_ship_simulation_}.apply_movement(inputs);
        collision_pipeline_.track_player_movement(
            player_ship_simulation_->unique_entity_id,
            before,
            player_ship_simulation_->get_physical_state().transform);
    }

    {
        FrameScratchScope scratch_scope{frame_memory_};
        fighters::ActionInputs const fighter_inputs{lasers_simulation_, frame_memory_};
        spinners::ActionInputs const spinner_inputs{lasers_simulation_};
        fighters_phase.apply_movement(fighter_inputs);
        spinners_phase.apply_movement(spinner_inputs);
    }

    collision_pipeline_.detect_and_handle_overlaps(get_read_access(),
                                                   fighters_simulation_.get_overlap_candidates(),
                                                   combat_events_,
                                                   frame_memory_);
}
void LevelSim::resolve_tick(bool const player_active) {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};

    SANDBOX_PROFILE_SCOPE("Resolution");
    clock_.transition_to(SimulationPhase::Resolution);

    {
        FrameScratchScope scratch_scope{frame_memory_};

        combat_events_.prepare(entity_tables_.lookups, &frame_memory_);

        capital_ships::ResolutionInputs const capital_inputs{combat_events_, clock_, frame_memory_};
        fighters::ResolutionInputs const fighter_inputs{combat_events_, frame_memory_};
        turrets::ResolutionInputs const turret_inputs{combat_events_, frame_memory_};

        if (player_active) {
            player::ResolutionInputs const inputs{combat_events_, frame_memory_};
            player::PhaseInterface{*player_ship_simulation_}.resolve_damage_events(inputs);
        }
        capital_ships_phase.resolve_damage_events(capital_inputs);
        fighters_phase.resolve_damage_events(fighter_inputs);
        turrets_phase.resolve_damage_events(turret_inputs);
        capital_ships_phase.resolve_fighters_of_dying_capitals();

        {
            SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
            capital_ships_phase.publish_deaths(capital_inputs);
            fighters_phase.publish_deaths();
            turrets_phase.publish_deaths();
        }
    }

    // Evaluate final health and deaths before invalidating row mappings.
    {
        FrameScratchScope scratch_scope{frame_memory_};
        mission_.tick({SimTick{clock_.completed_ticks.value() + 1},
                       entity_ledger_,
                       event_manager_.get_entity_ids(),
                       &collision_pipeline_.queries(),
                       &frame_memory_});
    }

    {
        FrameScratchScope scratch_scope{frame_memory_};

        SANDBOX_PROFILE_SCOPE("ResolutionCommit");
        clock_.transition_to(SimulationPhase::ResolutionCommit);

        // Apply the same removals to fixed health ranges and their owning rows.
        capital_ships_phase.remove_components();
        fighters_phase.remove_components();
        turrets_phase.remove_components();

        capital_ships_phase.remove_entities();
        fighters_phase.remove_entities();
        turrets_phase.remove_entities();
        lasers_phase.cleanup_entities();
        capital_ships_simulation_.refresh_fighter_ids(&frame_memory_);
    }

    capital_ships_phase.finish_action();
    fighters_phase.finish_action();
    turrets_phase.finish_action();
    spinners_phase.finish_action();
    lasers_phase.finish_action();
    clock_.completed_ticks.set_value(clock_.completed_ticks.value() + 1);
    level_telemetry_manager_.tick();
    combat_events_.reset();
    frame_memory_.reset();
}

/* **************************************** */
// Read views
/* **************************************** */

void LevelSim::refresh_spatial_index() {
    collision_pipeline_.refresh_spatial_index(get_read_access());
}
} // namespace ioj::sim

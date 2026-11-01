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
LevelSim::LevelSim(LevelSimInitData data, std::pmr::memory_resource& memory)
    : memory_manager_{memory, data.frame_memory_capacity_bytes}
    , collision_pipeline_{entity_tables_,
                          std::move(data.collision),
                          &memory_manager_.get_game_memory()}
    , lasers_simulation_{clock_, combat_events_}
    , fighters_simulation_{entity_tables_}
    , capital_ships_simulation_{entity_ledger_, entity_tables_}
    , turrets_simulation_{clock_, entity_ledger_, entity_tables_}
    , spinners_simulation_{clock_, entity_ledger_, entity_tables_}
    , mission_{std::move(data.mission)}
    , level_metadata_{std::move(data.level_metadata)}
    , event_manager_{capital_ships_simulation_,
                     fighters_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     data.capital_ships.fighter_spawn_slots_relative_transforms,
                     std::move(data.level_events)}
    , level_telemetry_manager_{clock_,
                               entity_ledger_,
                               lasers_simulation_,
                               memory_manager_.get_game_memory(),
                               data.telemetry_history} {
    clock_.initialise(data.clock_settings);
    set_telemetry_metadata(std::move(data.telemetry_metadata));

    initialise_subsystems(data);

    event_manager_.begin(player_ship_simulation_ ? player_ship_simulation_->unique_entity_id
                                                 : EntityId{});
    event_manager_.execute_tick(SimTick{});
}
void LevelSim::finish_initialisation() {
    assert(state_ == LevelState::Uninitialised);

    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_simulation_.update_entity_lookup_table();
    fighters::PhaseInterface{fighters_simulation_}.update_entity_lookup_table();
    turrets_simulation_.update_entity_lookup_table();
    spinners_simulation_.update_entity_lookup_table();
    refresh_spatial_index();

    state_ = LevelState::StableSetup;
    clock_.advance_to(SimulationPhase::Thinking);

    level_telemetry_manager_.initialise();
    mission_.reset_progress();

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
    participating_teams_ = event_manager_.get_participating_teams(player_team);
    fighters::PhaseInterface{fighters_simulation_}.initialise(
        data.fighters, {.participating_teams = participating_teams_, .clock = clock_});
    turrets::PhaseInterface{turrets_simulation_}.initialise(data.turrets);
    spinners::PhaseInterface{spinners_simulation_}.initialise(data.spinners);
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
void LevelSim::stop_run_if_requested(RunStopConditions const& conditions) {
    if (state_ != LevelState::Running) {
        return;
    }

    auto const has_future_spawns{conditions.stop_when_battle_resolved &&
                                 event_manager_.has_future_spawns()};
    if (auto const reason{conditions.stop_reason(clock_, entity_ledger_, has_future_spawns)}) {
        stop_run(*reason);
    }
}
auto LevelSim::get_player_team_completion() const -> std::optional<TeamMissionCompletion> {
    auto const team{player_ship_simulation_ ? std::optional{player_ship_simulation_->team}
                                            : std::nullopt};
    return mission_.get_team_completion(team);
}

auto LevelSim::take_finalized_telemetry_run() -> std::optional<telemetry::RunRecord> {
    return level_telemetry_manager_.take_finalized_run(clock_.get_tick_rate());
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt) {
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
            advance_tick<TickPlayerShipState::NotPresent>(tick_period);
        } else if (player_ship_simulation_->is_alive()) {
            advance_tick<TickPlayerShipState::PresentAlive>(tick_period);
        } else {
            advance_tick<TickPlayerShipState::PresentDead>(tick_period);
        }

        profiling::mark_frame("Simulation");
        clock_.transition_to(SimulationPhase::BetweenTicks);
        if (state_ != LevelState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Tick phases
/* **************************************** */
template <TickPlayerShipState PlayerState>
void LevelSim::advance_tick(float const tick_period) {
    prepare_tick<PlayerState>(tick_period);
    think_tick<PlayerState>(tick_period);
    act_tick<PlayerState>(tick_period);
    resolve_tick<PlayerState>();
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

    auto const previous_issued_counts{entity_ledger_.get_issued_counts()};
    event_manager_.execute_tick(SimTick{clock_.completed_ticks.value() + 1});

    fighters_phase.commit_spawns({.ledger = entity_ledger_});

    lasers_phase.commit_spawns();

    // Borrow fresh owner views after all spawn commits.
    collision_pipeline_.collect_spawn_candidates(
        previous_issued_counts,
        capital_ships_simulation_.get_entities().entity_ids(),
        fighters_simulation_.get_entities().entity_ids(),
        turrets_simulation_.get_entities().entity_ids());

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.prepare_tick(tick_period);
    } else if constexpr (PlayerState == TickPlayerShipState::PresentDead) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_phase.prepare_tick();
    fighters_phase.prepare_tick();
    turrets_phase.prepare_tick(tick_period);
    spinners_phase.prepare_tick(tick_period);

    refresh_spatial_index();
}
template <TickPlayerShipState PlayerState>
void LevelSim::think_tick(float const tick_period) {
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};

    auto& frame_memory{memory_manager_.get_frame_memory()};
    FrameScratchScope scratch_scope{frame_memory};

    SANDBOX_PROFILE_SCOPE("Thinking");
    clock_.transition_to(SimulationPhase::Thinking);

    task_manager_.plan_tasks({.active_teams = participating_teams_, .mission = mission_});

    event_manager_.queue_fighter_spawns(tick_period, frame_memory);

    turrets::ThinkingInputs const turret_inputs{
        collision_pipeline_.queries(), lasers_simulation_, frame_memory};
    fighters::ThinkingInputs const fighter_inputs{
        .clock = clock_,
        .collision_radius =
            collision_pipeline_.queries().get_entity_type_radius(EntityType::Fighter),
        .spatial_query_manager = collision_pipeline_.queries(),
        .scratch_resource = frame_memory};

    turrets_phase.think(turret_inputs);
    fighters_phase.think(fighter_inputs);
    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.think(tick_period);
    }
    spinners_phase.think(tick_period);

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        player::PhaseInterface{*player_ship_simulation_}.generate_fire_commands();
    }
    fighters_phase.generate_fire_commands(fighter_inputs);
    turrets_phase.generate_fire_commands(turret_inputs);
    spinners_phase.generate_fire_commands();
}
template <TickPlayerShipState PlayerState>
void LevelSim::act_tick(float const tick_period) {
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};
    auto& frame_memory{memory_manager_.get_frame_memory()};

    SANDBOX_PROFILE_SCOPE("Action");
    clock_.transition_to(SimulationPhase::Action);

    {
        FrameScratchScope scratch_scope{frame_memory};
        lasers::ActionInputs const inputs{tick_period, collision_pipeline_.queries(), frame_memory};
        lasers_phase.simulate(inputs);
    }

    if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
        auto const before{player_ship_simulation_->get_physical_state().transform};
        player::ActionInputs const inputs{lasers_simulation_};
        player::PhaseInterface{*player_ship_simulation_}.apply_movement(inputs);
        collision_pipeline_.track_player_movement(
            player_ship_simulation_->unique_entity_id,
            before,
            player_ship_simulation_->get_physical_state().transform);
    }

    {
        FrameScratchScope scratch_scope{frame_memory};
        fighters::ActionInputs const fighter_inputs{tick_period, lasers_simulation_, frame_memory};
        spinners::ActionInputs const spinner_inputs{lasers_simulation_};
        fighters_phase.apply_movement(fighter_inputs);
        spinners_phase.apply_movement(spinner_inputs);
    }

    collision_pipeline_.detect_and_handle_overlaps(get_read_access(),
                                                   fighters_simulation_.get_overlap_candidates(),
                                                   combat_events_,
                                                   frame_memory);
}
template <TickPlayerShipState PlayerState>
void LevelSim::resolve_tick() {
    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};
    auto& frame_memory{memory_manager_.get_frame_memory()};

    SANDBOX_PROFILE_SCOPE("Resolution");
    clock_.transition_to(SimulationPhase::Resolution);

    {
        FrameScratchScope scratch_scope{frame_memory};

        combat_events_.prepare(entity_tables_.lookups, &frame_memory);

        capital_ships::ResolutionInputs const capital_inputs{combat_events_, clock_, frame_memory};
        fighters::ResolutionInputs const fighter_inputs{.combat_events = combat_events_,
                                                        .ledger = entity_ledger_,
                                                        .tick = clock_.get_completed_ticks(),
                                                        .scratch_resource = frame_memory};
        turrets::ResolutionInputs const turret_inputs{combat_events_, frame_memory};

        if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
            player::ResolutionInputs const inputs{combat_events_, frame_memory};
            player::PhaseInterface{*player_ship_simulation_}.resolve_damage_events(inputs);
        }
        capital_ships_phase.resolve_damage_events(capital_inputs);
        fighters_phase.resolve_damage_events(fighter_inputs);
        turrets_phase.resolve_damage_events(turret_inputs);

        {
            SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
            capital_ships_phase.publish_deaths(capital_inputs);
            fighters_phase.publish_deaths(fighter_inputs);
            turrets_phase.publish_deaths();
        }
    }

    // Evaluate final health and deaths before invalidating row mappings.
    {
        FrameScratchScope scratch_scope{frame_memory};
        mission_.evaluate({SimTick{clock_.completed_ticks.value() + 1},
                           entity_ledger_,
                           event_manager_.get_entity_ids(),
                           &collision_pipeline_.queries(),
                           &frame_memory,
                           participating_teams_});
    }

    {
        FrameScratchScope scratch_scope{frame_memory};

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
    }

    capital_ships_phase.finish_action();
    fighters_phase.finish_action();
    turrets_phase.finish_action();
    spinners_phase.finish_action();
    lasers_phase.finish_action();
    clock_.completed_ticks.set_value(clock_.completed_ticks.value() + 1);
    level_telemetry_manager_.tick();
    combat_events_.reset();
    frame_memory.reset();
}

/* **************************************** */
// Read views
/* **************************************** */

void LevelSim::refresh_spatial_index() {
    collision_pipeline_.refresh_spatial_index(get_read_access());
}
} // namespace ioj::sim

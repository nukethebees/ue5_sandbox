#include <ioj/sim/levels/level_sim.h>

#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/expressions/expr_evaluation_context.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/lasers/phase_interface.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/player/phase_interface.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spinners/phase_interface.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/turrets/phase_interface.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <utility>

namespace ioj::sim {

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(LevelSimInitData data, std::pmr::memory_resource& memory)
    : clock_{data.clock_settings}
    , memory_manager_{
          memory,
          data.tick_memory_capacity_bytes,
          data.phase_memory_capacity_bytes,
      }
    , overlap_handler_{data.collision.overlap_response.damage_per_overlap_detection}
    , overlap_candidates_{&memory_manager_.game_memory()}
    , capital_ships_simulation_{entity_tables_}
    , turrets_simulation_{entity_tables_}
    , spinners_simulation_{entity_tables_}
    , task_manager_{memory_manager_.game_memory()}
    , conditions_{std::move(data.conditions)}
    , timers_{std::move(data.timers)}
    , level_metadata_{std::move(data.level_metadata)}
    , event_manager_{std::move(data.level_events), memory_manager_.level_memory()}
    , spawn_manager_{
          data.capital_ships.fighter_spawn_slots_relative_transforms.size(),
          data.fighters.max_live_auto_spawned_fighters.get(),
          data.participating_team_count,
          data.fighters.avoidance_clearance_buffer,
          memory_manager_.game_memory(),
      }
    {
    spatial_queries_.initialise(data.collision.grid_spec, data.collision.entity_bounds);
    spatial_queries_.set_static_collision(std::move(data.collision.static_bounds));
    initialise_subsystems(data);

    spawn_manager_.initialise(event_manager_.spawn_count() +
                              static_cast<std::uint32_t>(player_ship_simulation_.has_value()));
    if (data.player_entity_ref.is_valid()) {
        spawn_manager_.set_entity_id(data.player_entity_ref,
                                     player_ship_simulation_->unique_entity_id);
    }

    event_manager_.dispatch_preparation(spawn_manager_, level_spawn_context());

    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships::InitialisationInterface{capital_ships_simulation_}.update_entity_lookup_table(
        clock_);
    fighters::InitialisationInterface{fighters_simulation_}.update_entity_lookup_table(
        entity_tables_, clock_);
    turrets::InitialisationInterface{turrets_simulation_}.update_entity_lookup_table(clock_);
    spinners::InitialisationInterface{spinners_simulation_}.update_entity_lookup_table(clock_);
    refresh_spatial_index();

    state_ = LevelState::StableSetup;
    clock_.advance_to(SimulationPhase::Thinking);

    clock_.reset();
    state_ = LevelState::Paused;
    event_manager_.dispatch_tick_end(timers_, clock_, state_, terminal_);
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
auto LevelSim::level_spawn_context() -> LevelSpawnContext {
    return {capital_ships_simulation_,
            turrets_simulation_,
            spinners_simulation_,
            fighters_simulation_,
            {entity_tables_, clock_, entity_ledger_, task_manager_}};
}
void LevelSim::initialise_subsystems(LevelSimInitData const& data) {
    if (data.player) {
        auto& simulation{player_ship_simulation_.emplace(clock_, entity_ledger_, entity_tables_)};
        player::InitialisationInterface{simulation}.initialise(*data.player);
    }

    capital_ships::InitialisationInterface{capital_ships_simulation_}.initialise(
        data.capital_ships.max_health);
    fighters::InitialisationInterface{fighters_simulation_}.initialise(data.fighters, clock_);
    turrets::InitialisationInterface{turrets_simulation_}.initialise(data.turrets, clock_);
    spinners::InitialisationInterface{spinners_simulation_}.initialise(data.spinners, clock_);
    lasers::InitialisationInterface{lasers_simulation_}.initialise(data.lasers);
}

/* **************************************** */
// Level results
/* **************************************** */
void LevelSim::stop_run(RunEndReason const reason) {
    if (state_ == LevelState::Stopped) {
        return;
    }
    event_manager_.enqueue_stop(reason);
    event_manager_.dispatch_tick_end(timers_, clock_, state_, terminal_);
}
auto LevelSim::result() const -> std::optional<LevelResult> {
    if (!terminal_) {
        return std::nullopt;
    }
    auto const event{terminal_->event};
    if (event.kind() == EventKind::Win) {
        return LevelResult{
            *terminal_, event_manager_.win_event(event).winner, RunEndReason::LevelCompleted};
    }
    return LevelResult{*terminal_, std::nullopt, event_manager_.stop_event(event)};
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt, fighters::NavigationCounters* const navigation_output) {
    SANDBOX_PROFILE_SCOPE("LevelSim::advance");
    if (state_ == LevelState::Stopped) {
        return;
    }

    event_manager_.dispatch_tick_end(timers_, clock_, state_, terminal_);
    if (state_ != LevelState::Running) {
        return;
    }

    entity_ledger_.reset_frame_output();
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    clock_.add_time(dt);

    while (clock_.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_period())};
        if (!player_ship_simulation_) {
            advance_tick<TickPlayerShipState::NotPresent>(tick_period, navigation_output);
        } else if (player_ship_simulation_->is_alive()) {
            advance_tick<TickPlayerShipState::PresentAlive>(tick_period, navigation_output);
        } else {
            advance_tick<TickPlayerShipState::PresentDead>(tick_period, navigation_output);
        }

        profiling::mark_frame("Simulation");
        event_manager_.dispatch_tick_end(timers_, clock_, state_, terminal_);
        if (state_ != LevelState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Tick phases
/* **************************************** */
template <TickPlayerShipState PlayerState>
void LevelSim::advance_tick(float const tick_period,
                            fighters::NavigationCounters* const navigation_output) {
    {
        capital_ships::PreparationInterface capital_ships_phase{capital_ships_simulation_};
        fighters::PreparationInterface fighters_phase{fighters_simulation_};
        turrets::PreparationInterface turrets_phase{turrets_simulation_};
        spinners::PreparationInterface spinners_phase{spinners_simulation_};
        lasers::PreparationInterface lasers_phase{lasers_simulation_};

        SANDBOX_PROFILE_SCOPE("Preparation");
        clock_.transition_to(SimulationPhase::Preparation);

        auto const previous_issued_counts{entity_ledger_.issued_counts()};
        auto const spawn_context{level_spawn_context()};
        event_manager_.dispatch_preparation(spawn_manager_, spawn_context);
        spawn_manager_.commit_carrier_launches(spawn_context);

        lasers_phase.commit_spawns({clock_});

        // Borrow fresh owner views after all spawn commits.
        collect_spawn_candidates(previous_issued_counts);

        if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
            player::PreparationInterface{*player_ship_simulation_}.prepare_tick(tick_period);
        } else if constexpr (PlayerState == TickPlayerShipState::PresentDead) {
            player_ship_simulation_->update_entity_lookup_table();
        }
        capital_ships_phase.prepare_tick(clock_);
        fighters_phase.prepare_tick(spawn_context.fighter_inputs);
        turrets_phase.prepare_tick(clock_);
        spinners_phase.prepare_tick(tick_period, clock_);

        refresh_spatial_index();
    }

    {
        std::pmr::vector<lasers::LaserIndex> laser_removal_indices{&memory_manager_.tick_memory()};
        {
            FrameArray<std::uint32_t> fighter_fire_indices{&memory_manager_.tick_memory()};
            FrameArray<std::uint32_t> spinner_fire_indices{&memory_manager_.tick_memory()};

            {
                fighters::ThinkingInterface fighters_phase{fighters_simulation_};
                turrets::ThinkingInterface turrets_phase{turrets_simulation_};
                spinners::ThinkingInterface spinners_phase{spinners_simulation_};

                auto& phase_memory{memory_manager_.phase_memory()};
                FrameScratchScope scratch_scope{phase_memory};

                SANDBOX_PROFILE_SCOPE("Thinking");
                clock_.transition_to(SimulationPhase::Thinking);

                task_manager_.allocate({
                    .level = read_access(),
                    .events = event_manager_,
                    .conditions = conditions_,
                    .entities = spawn_manager_.entity_ids(),
                    .executors = entity_tables_.executors,
                });

                spawn_manager_.queue_fighter_spawns(
                    tick_period,
                    capital_ships_simulation_.get_entities(),
                    entity_tables_.lookups.for_type(EntityType::CapitalShip).entries(),
                    spatial_queries_.local_entity_bounds());

                turrets::ThinkingInputs const turret_inputs{
                    clock_, spatial_queries_, lasers_simulation_, phase_memory};
                fighters::NavigationCounters navigation_counters{};
                fighters::ThinkingInputs const fighter_inputs{
                    .entity_tables = entity_tables_,
                    .clock = clock_,
                    .task_manager = task_manager_,
                    .collision_radius = spatial_queries_.entity_type_radius(EntityType::Fighter),
                    .spatial_query_manager = spatial_queries_,
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

            {
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
                        damage_queue_,
                        clock_.get_completed_ticks(),
                        spatial_queries_,
                        phase_memory,
                        laser_removal_indices,
                    };
                    lasers_phase.simulate(inputs);
                }

                if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
                    auto const before{player_ship_simulation_->physical_state().transform};
                    player::ActionInputs const inputs{lasers_simulation_};
                    player::ActionInterface{*player_ship_simulation_}.apply_movement(inputs);
                    track_player_movement(player_ship_simulation_->unique_entity_id,
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
                    spinners::ActionInputs const spinner_inputs{lasers_simulation_,
                                                                spinner_fire_indices};
                    fighters_phase.apply_movement(fighter_inputs);
                    spinners_phase.apply_movement(spinner_inputs);
                }

                detect_and_handle_overlaps(fighter_overlap_candidates, phase_memory);
            }
        }

        {
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

                damage_queue_.prepare(entity_tables_.lookups, &phase_memory);
                assert(std::ranges::all_of(damage_queue_.all_events().const_view().damage_amounts(),
                                           [](auto amount) { return amount >= 0; }));

                capital_ships::ResolutionInputs const capital_inputs{
                    damage_queue_,
                    entity_ledger_,
                    clock_.get_completed_ticks(),
                    capital_removal_indices,
                    capital_deaths,
                };
                fighters::ResolutionInputs const fighter_inputs{
                    .entity_tables = entity_tables_,
                    .damage_queue = damage_queue_,
                    .ledger = entity_ledger_,
                    .task_manager = task_manager_,
                    .tick = clock_.get_completed_ticks(),
                    .local_indices_to_remove = fighter_removal_indices,
                    .entity_death_info = fighter_deaths,
                };
                turrets::ResolutionInputs const turret_inputs{
                    damage_queue_,
                    entity_ledger_,
                    clock_.get_completed_ticks(),
                    turret_removal_indices,
                    turret_deaths,
                };

                if constexpr (PlayerState == TickPlayerShipState::PresentAlive) {
                    player::ResolutionInputs const inputs{damage_queue_};
                    player::ResolutionInterface{*player_ship_simulation_}.resolve_damage_events(
                        inputs);
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
                conditions_.commit_latches();
                timers_.tick(1);
                assert(is_lookup_allowed(clock_));
                conditions_.evaluate({entity_ledger_,
                                      timers_,
                                      spawn_manager_.entity_ids(),
                                      spatial_queries_,
                                      entity_tables_.lookups,
                                      phase_memory});
                event_manager_.collect_ready(conditions_);
            }

            capital_ships::ResolutionCommitInterface capital_ships_commit{
                capital_ships_simulation_};
            fighters::ResolutionCommitInterface fighters_commit{fighters_simulation_};
            turrets::ResolutionCommitInterface turrets_commit{turrets_simulation_};
            lasers::ResolutionCommitInterface lasers_commit{lasers_simulation_};

            {
                FrameScratchScope scratch_scope{phase_memory};

                SANDBOX_PROFILE_SCOPE("ResolutionCommit");
                clock_.transition_to(SimulationPhase::ResolutionCommit);
                fighters::ResolutionCommitInputs const fighter_inputs{
                    entity_tables_, clock_, fighter_removal_indices};
                capital_ships::ResolutionCommitInputs const capital_inputs{clock_,
                                                                           capital_removal_indices};
                turrets::ResolutionCommitInputs const turret_inputs{clock_, turret_removal_indices};

                capital_ships_commit.remove_entities(capital_inputs);
                fighters_commit.remove_entities(fighter_inputs);
                turrets_commit.remove_entities(turret_inputs);
                lasers_commit.cleanup_entities({laser_removal_indices, clock_});
            }

            capital_ships_commit.finish_action();
            fighters_commit.finish_action();
            turrets_commit.finish_action();
            lasers_commit.finish_action();
            clock_.complete_tick();
            damage_queue_.reset();
        }
    }
    memory_manager_.tick_memory().reset();
    memory_manager_.phase_memory().reset();
}

/* **************************************** */
// Spatial queries and collision damage
/* **************************************** */

void LevelSim::refresh_spatial_index() {
    spatial_queries_.refresh_spatial_index(read_access());
}
void LevelSim::collect_spawn_candidates(EntityTypeSizes const& previous_issued_counts) {
    auto const capitals{capital_ships_simulation_.get_entities().entity_ids()};
    auto const fighters{fighters_simulation_.entities().entity_ids()};
    auto const turrets{turrets_simulation_.get_entities().entity_ids()};
    overlap_candidates_.clear();
    for (auto const ids : std::array{capitals, fighters, turrets}) {
        for (auto const id : ids) {
            if (id.index() >= previous_issued_counts[id.entity_type()]) {
                overlap_candidates_.push_back(id);
            }
        }
    }
}
void LevelSim::track_player_movement(EntityId const player,
                                     Transform3d const& before,
                                     Transform3d const& after) {
    auto const before_location{Vector3f{before.location}};
    auto const after_location{Vector3f{after.location}};
    auto const before_rotation{Rotator3f{before.rotator()}};
    auto const after_rotation{Rotator3f{after.rotator()}};
    if (before_location != after_location || before_rotation != after_rotation) {
        overlap_candidates_.push_back(player);
    }
}
void LevelSim::detect_and_handle_overlaps(std::span<EntityId const> const moved_fighters,
                                          FrameMemoryResource& scratch) {
    assert(is_lookup_allowed(clock_));

    SANDBOX_PROFILE_SCOPE("Refresh spatial index and detect overlaps");

    overlap_candidates_.insert(
        overlap_candidates_.end(), moved_fighters.begin(), moved_fighters.end());
    std::ranges::sort(overlap_candidates_);
    auto const duplicates{std::ranges::unique(overlap_candidates_)};
    overlap_candidates_.erase(duplicates.begin(), duplicates.end());

    refresh_spatial_index();

    collision::EntityEntityOverlaps entity_overlaps{scratch};
    collision::EntityStaticOverlaps static_overlaps{scratch};
    spatial_queries_.detect_overlaps(
        entity_tables_.lookups, overlap_candidates_, entity_overlaps, static_overlaps, &scratch);
    overlap_handler_.handle({entity_overlaps.const_view(), static_overlaps.const_view()},
                            damage_queue_);
}

} // namespace ioj::sim

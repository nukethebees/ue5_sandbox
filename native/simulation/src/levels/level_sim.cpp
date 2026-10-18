#include <ioj/sim/levels/level_sim.h>

#include <ioj/diagnostics.h>
#include <ioj/sim/capital_ships/phase_interface.h>
#include <ioj/sim/fighters/phase_interface.h>
#include <ioj/sim/lasers/phase_interface.h>
#include <ioj/sim/player/phase_interface.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spinners/phase_interface.h>
#include <ioj/sim/turrets/phase_interface.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {

namespace level_simulation {
/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(LevelSimInitData& data) {
    std::array<std::uint8_t, ioj::enum_count<Team>()> included{};
    auto const included_count{included.size()};
    auto include = [&included](Team const team) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < included_count) {
            included[team_index] = 1;
        } else {
            ioj::log_error(std::format("Ignoring invalid participating team {}", team_index));
        }
    };

    for (auto const team : data.participating_teams) {
        include(team);
    }

    if (data.participating_teams.is_empty()) {
        // Collect teams from the player and all spawn payloads.
        if (data.player.has_value()) {
            include(data.player->team);
        }
        for (auto const team : data.level_events.schedule.capital_spawns.get_const_view().teams()) {
            include(team);
        }
        for (auto const team : data.level_events.schedule.turret_spawns.get_const_view().teams()) {
            include(team);
        }
    }

    // Rebuild participating teams
    data.participating_teams.reset();
    for (std::int32_t team_index{}; static_cast<std::size_t>(team_index) < included_count;
         ++team_index) {
        if (included[team_index] != 0) {
            data.participating_teams.add(static_cast<Team>(team_index));
        }
    }
}

}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(GameMemory& game_memory, LevelSimInitData data)
    : frame_memory_block_{game_memory.acquire_block(data.frame_memory_capacity_bytes,
                                                    ioj::FrameMemoryResource::backing_alignment)}
    , frame_memory_{std::span<std::byte>{frame_memory_block_.data(),
                                         frame_memory_block_.size_bytes()}}
    , query_manager_{entity_tables_, &game_memory.memory_resource()}
    , overlap_handler_{data.overlap_response}
    , lasers_simulation_{clock_, combat_events_, query_manager_}
    , fighters_simulation_{clock_,
                           entity_ledger_,
                           combat_events_,
                           entity_tables_,
                           query_manager_,
                           lasers_simulation_}
    , capital_ships_simulation_{clock_,
                                entity_ledger_,
                                combat_events_,
                                entity_tables_,
                                query_manager_,
                                fighters_simulation_}
    , turrets_simulation_{clock_,
                          entity_ledger_,
                          combat_events_,
                          entity_tables_,
                          query_manager_,
                          lasers_simulation_}
    , spinners_simulation_{clock_, entity_ledger_, entity_tables_, lasers_simulation_}
    , mission_manager_{clock_, entity_ledger_, entity_tables_}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     mission_manager_}
    , level_telemetry_manager_{
          clock_, entity_ledger_, lasers_simulation_, game_memory, data.telemetry_history} {
    // Initialise timing and metadata
    clock_.initialise(data.clock_settings);
    telemetry_metadata_ = std::move(data.telemetry_metadata);
    level_simulation::finalise_participating_teams(data);

    // Configure queries and subsystems
    initialise_spatial_queries(data);
    configure_subsystems(data);
    begin_subsystems();

    // Run initial events
    initialise_events(std::move(data.level_events));
    event_manager_.execute_tick(0);
}
void LevelSim::finish_initialisation() {
    assert(state_ == OrchestratorState::Uninitialised);

    // Build indexes and query state
    if (player_ship_simulation_) {
        player_ship_simulation_->update_entity_lookup_table();
    }
    capital_ships_simulation_.update_entity_lookup_table();
    fighters_simulation_.update_entity_lookup_table();
    turrets_simulation_.update_entity_lookup_table();
    spinners_simulation_.update_entity_lookup_table();
    refresh_spatial_index();

    clock_.transition_to(SimulationPhase::StableSetup);

    // Start telemetry and mission
    initialise_telemetry();
    event_manager_.configure_mission();
    {
        ioj::FrameScratchScope scratch_scope{frame_memory_};
        mission_manager_.begin_play(&frame_memory_);
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
void LevelSim::set_time_scale(time_type scale) {
    assert(scale > 0.0);
    clock_.tick_loop.time_scale = scale;
}

/* **************************************** */
// Subsystem setup
/* **************************************** */
void LevelSim::configure_subsystems(LevelSimInitData const& data) {
    // Configure diagnostics and player
    set_fighter_diagnostics_enabled(data.fighter_diagnostics_enabled);
    if (data.player.has_value()) {
        configure_player(data.player.value());
    }

    // Configure subsystems
    lasers_simulation_.set_config(data.lasers);
    capital_ships_simulation_.set_config(data.capital_ships);
    fighters_simulation_.set_config(
        data.fighters,
        {.participating_teams = data.participating_teams,
         .collision_radius = query_manager_.get_entity_type_radius(EntityType::Fighter),
         .fire_point_distance = data.fighter_fire_point_distance});
    turrets_simulation_.set_config(data.turrets);
    spinners_simulation_.set_config(data.spinners);
}
void LevelSim::configure_player(player::PlayerSpawnData const& spawn) {
    auto& player{player_ship_simulation_.emplace(clock_,
                                                 entity_ledger_,
                                                 combat_events_,
                                                 entity_tables_,
                                                 query_manager_,
                                                 lasers_simulation_)};
    player_ship_commands_.emplace(player);

    player.configure(spawn);
}
void LevelSim::initialise_spatial_queries(LevelSimInitData& data) {
    // Configure collision queries and buffers
    query_manager_.initialise(data.grid_geometry, data.entity_bounds);

    // Install static bounds
    query_manager_.set_static_collision(std::move(data.static_bounds));
}
void LevelSim::begin_subsystems() {
    // Start player subsystem
    if (player_ship_simulation_.has_value()) {
        player::PhaseInterface{*player_ship_simulation_}.begin_play();
    }

    // Start world subsystems
    capital_ships::PhaseInterface{capital_ships_simulation_}.begin_play();
    fighters::PhaseInterface{fighters_simulation_}.begin_play();
    turrets::PhaseInterface{turrets_simulation_}.begin_play();

    spinners::PhaseInterface{spinners_simulation_}.begin_play();
    lasers::PhaseInterface{lasers_simulation_}.begin_play();
}
void LevelSim::initialise_events(CompiledLevelEvents events) {
    auto const player_id{player_ship_simulation_.has_value()
                             ? player_ship_simulation_->unique_entity_id
                             : EntityId{}};
    event_manager_.initialise(std::move(events), player_id);
}
/* **************************************** */
// Configuration and commands
/* **************************************** */
void LevelSim::set_fighter_diagnostics_enabled(bool const enabled) noexcept {
    capital_ships_simulation_.set_diagnostics_enabled(enabled);
    fighters_simulation_.set_diagnostics_enabled(enabled);
}
void LevelSim::set_static_collision(collision::WorldAABBs bounds) {
    assert(state_ == OrchestratorState::Uninitialised);
    query_manager_.set_static_collision(std::move(bounds));
}
auto LevelSim::add_static_collision_aabb(Vector3f const min_point, Vector3f const max_point)
    -> collision::StaticGeometryIndex {
    return query_manager_.add_static_collision_aabb(min_point, max_point);
}

/* **************************************** */
// Telemetry and mission results
/* **************************************** */
void LevelSim::initialise_telemetry() {
    level_telemetry_manager_.initialise();

    if (telemetry_metadata_.has_value()) {
        level_telemetry_manager_.begin_run(std::move(telemetry_metadata_.value()));
        telemetry_metadata_.reset();
    }
}
void LevelSim::finalize_telemetry_run(LevelTelemetryRunEndReason const reason, std::string detail) {
    level_telemetry_manager_.finalize_interrupted(reason, std::move(detail));
}
void LevelSim::complete_telemetry_run(LevelTelemetryRunEndReason const reason,
                                      std::optional<Team> winning_team) {
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
auto LevelSim::take_finalized_telemetry_run() -> std::optional<LevelTelemetryRunRecord> {
    return level_telemetry_manager_.take_finalized_run();
}

/* **************************************** */
// Simulation
/* **************************************** */
void LevelSim::advance(time_type const dt) {
    SANDBOX_PROFILE_SCOPE("LevelSim::advance");

    if (state_ != OrchestratorState::Running) {
        return;
    }

    capital_ships::PhaseInterface capital_ships_phase{capital_ships_simulation_};
    fighters::PhaseInterface fighters_phase{fighters_simulation_};
    turrets::PhaseInterface turrets_phase{turrets_simulation_};
    spinners::PhaseInterface spinners_phase{spinners_simulation_};
    lasers::PhaseInterface lasers_phase{lasers_simulation_};
    std::optional<player::PhaseInterface> player_ship_phase;
    if (player_ship_simulation_) {
        player_ship_phase.emplace(*player_ship_simulation_);
    }

    ++frame_sequence_;
    entity_ledger_.reset_frame_output();
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    query_manager_.reset_frame_collision_events();
    clock_.tick_loop.add_time(dt);

    auto publish_entity_deaths{[&] {
        SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
        capital_ships_phase.publish_deaths();
        fighters_phase.publish_deaths();
        turrets_phase.publish_deaths();
    }};

    while (clock_.tick_loop.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_loop.get_tick_period())};
        auto const player_active{player_ship_simulation_.has_value() &&
                                 player_ship_simulation_->is_alive()};

        /* -------------------------------------------------------------------------------- */
        // Preparation
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Preparation");
            clock_.transition_to(SimulationPhase::Preparation);

            // Commit orders and events
            fighters_phase.commit_orders();
            auto const previous_issued_counts{entity_ledger_.get_issued_counts()};
            event_manager_.execute_tick(clock_.completed_ticks + 1);

            // Commit spawns
            auto const previous_fighter_count{fighters_simulation_.get_num_instances()};
            fighters_phase.commit_spawns();
            capital_ships_simulation_.fighters_spawned +=
                fighters_simulation_.get_num_instances() - previous_fighter_count;

            lasers_phase.commit_spawns();

            // Track new collision entities
            overlap_candidates_.clear();
            auto collect_new = [&](auto const data) {
                for (auto const id : data.entity_ids()) {
                    if (id.index() >= previous_issued_counts[id.entity_type()]) {
                        overlap_candidates_.push_back(id);
                    }
                }
            };

            // Commits in this tick can invalidate the previous tick's entity views.
            // NOLINTBEGIN(ioj-loop-view-accessor-call)
            collect_new(capital_ships_simulation_.get_entities());
            collect_new(fighters_simulation_.get_entities());
            collect_new(turrets_simulation_.get_entities());
            // NOLINTEND(ioj-loop-view-accessor-call)

            // Prepare phases and indexes
            if (player_active) {
                player_ship_phase->prepare_tick(tick_period);
            } else if (player_ship_simulation_) {
                player_ship_simulation_->update_entity_lookup_table();
            }
            capital_ships_phase.prepare_tick(tick_period);
            fighters_phase.prepare_tick(tick_period);
            turrets_phase.prepare_tick(tick_period);
            spinners_phase.prepare_tick(tick_period);

            refresh_spatial_index();
        }

        /* -------------------------------------------------------------------------------- */
        // Thinking
        /* -------------------------------------------------------------------------------- */
        {
            ioj::FrameScratchScope scratch_scope{frame_memory_};
            auto* const scratch_resource{&frame_memory_};

            SANDBOX_PROFILE_SCOPE("Thinking");
            clock_.transition_to(SimulationPhase::Thinking);

            // Resolve dependent reads after every entity type has updated its lookup table.
            mission_manager_.prepare_objectives(scratch_resource);
            capital_ships_simulation_.refresh_fighter_ids(scratch_resource);

            // Run decision phases
            turrets_phase.think(tick_period, scratch_resource);
            capital_ships_phase.think(tick_period, scratch_resource);
            fighters_phase.think(tick_period, scratch_resource);
            if (player_active) {
                player_ship_phase->think(tick_period);
            }
            spinners_phase.think(tick_period);

            // Generate fire commands
            if (player_active) {
                player_ship_phase->generate_fire_commands();
            }
            fighters_phase.generate_fire_commands(scratch_resource);
            turrets_phase.generate_fire_commands(scratch_resource);
            spinners_phase.generate_fire_commands();
        }

        /* -------------------------------------------------------------------------------- */
        // Action
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Action");
            clock_.transition_to(SimulationPhase::Action);

            // Simulate projectiles
            {
                ioj::FrameScratchScope scratch_scope{frame_memory_};
                lasers_phase.simulate(tick_period, &frame_memory_);
            }

            // Track player movement
            if (player_active) {
                auto const before{player_ship_simulation_->get_physical_state().transform};
                player_ship_phase->apply_movement();
                auto const after{player_ship_simulation_->get_physical_state().transform};
                auto const before_location{to_float(before.location)};
                auto const after_location{to_float(after.location)};
                auto const before_rotation{to_float(before.rotator())};
                auto const after_rotation{to_float(after.rotator())};
                if (before_location.x() != after_location.x() ||
                    before_location.y() != after_location.y() ||
                    before_location.z() != after_location.z() ||
                    before_rotation.pitch != after_rotation.pitch ||
                    before_rotation.yaw != after_rotation.yaw ||
                    before_rotation.roll != after_rotation.roll) {
                    overlap_candidates_.push_back(player_ship_simulation_->unique_entity_id);
                }
            }

            // Move dynamic entities
            {
                ioj::FrameScratchScope scratch_scope{frame_memory_};
                auto* const scratch_resource{&frame_memory_};
                fighters_phase.apply_movement(scratch_resource);
                spinners_phase.apply_movement(scratch_resource);
            }

            {
                SANDBOX_PROFILE_SCOPE("Refresh spatial index and detect overlaps");

                // Movement rebuilds this tick's candidate set.
                // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                auto const fighter_candidates{fighters_simulation_.get_overlap_candidates()};
                overlap_candidates_.insert(overlap_candidates_.end(),
                                           fighter_candidates.begin(),
                                           fighter_candidates.end());
                std::ranges::sort(overlap_candidates_);
                auto const duplicates{std::ranges::unique(overlap_candidates_)};
                overlap_candidates_.erase(duplicates.begin(), duplicates.end());

                refresh_spatial_index();
                ioj::FrameScratchScope scratch_scope{frame_memory_};
                auto const overlaps{
                    // Detection creates a new scratch-backed result for this tick.
                    // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                    query_manager_.detect_overlaps(overlap_candidates_, &frame_memory_)};
                overlap_handler_.handle(overlaps, combat_events_);
            }
        }

        /* -------------------------------------------------------------------------------- */
        // Resolution
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Resolution");
            clock_.transition_to(SimulationPhase::Resolution);

            {
                ioj::FrameScratchScope scratch_scope{frame_memory_};

                // Prepare combat events
                combat_events_.prepare(entity_tables_.lookups, &frame_memory_);

                // Resolve damage
                if (player_active) {
                    player_ship_phase->resolve_damage_events(&frame_memory_);
                }
                capital_ships_phase.resolve_damage_events(&frame_memory_);
                fighters_phase.resolve_damage_events(&frame_memory_);
                turrets_phase.resolve_damage_events(&frame_memory_);
                capital_ships_phase.resolve_fighters_of_dying_capitals();

                // Publish deaths
                publish_entity_deaths();
            }

            // Evaluate final health and deaths before invalidating row mappings.
            {
                ioj::FrameScratchScope scratch_scope{frame_memory_};
                mission_manager_.mission_tick(&frame_memory_);
            }

            // Clean up entities and indexes
            {
                ioj::FrameScratchScope scratch_scope{frame_memory_};

                SANDBOX_PROFILE_SCOPE("ResolutionCommit");
                clock_.transition_to(SimulationPhase::ResolutionCommit);

                // Apply the same removals to fixed health ranges and their owning rows.
                capital_ships_phase.remove_components();
                fighters_phase.remove_components();
                turrets_phase.remove_components();

                // Retire dead identities and compact owning storage.
                capital_ships_phase.remove_entities();
                fighters_phase.remove_entities();
                turrets_phase.remove_entities();
                lasers_phase.cleanup_entities();
                capital_ships_simulation_.refresh_fighter_ids(&frame_memory_);
            }

            // Finish tick and reset frame state
            capital_ships_phase.finish_action();
            fighters_phase.finish_action();
            turrets_phase.finish_action();
            spinners_phase.finish_action();
            lasers_phase.finish_action();
            ++clock_.completed_ticks;
            level_telemetry_manager_.tick();
            combat_events_.reset();
            frame_memory_.reset();
        }
        profiling::mark_frame("Simulation");
        clock_.transition_to(SimulationPhase::BetweenTicks);

        if (state_ != OrchestratorState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Read views
/* **************************************** */

void LevelSim::refresh_spatial_index() {
    query_manager_.refresh_spatial_index(get_read_access());
}
} // namespace ioj::sim

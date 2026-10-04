#include <ioj/sim/level_sim.h>

#include <ioj/sim/profiling.h>

#include <sandbox/core/diagnostics.h>

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
    std::array<std::uint8_t, ml::enum_count<Team>()> included{};
    auto const included_count{included.size()};
    auto include = [&included](Team const team) {
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
        // Collect initial spawn teams
        if (data.player.has_value()) {
            include(data.player->team);
        }
        for (auto const team :
             data.level_events.initial_spawns.capital_spawns.get_const_view().teams()) {
            include(team);
        }
        for (auto const team :
             data.level_events.initial_spawns.turret_spawns.get_const_view().teams()) {
            include(team);
        }

        // Collect scheduled spawn teams
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
LevelSim::LevelSim(LevelSimInitData data)
    : local_game_memory_{data.game_memory == nullptr ? std::make_unique<GameMemory>() : nullptr}
    , game_memory_{data.game_memory != nullptr ? data.game_memory : local_game_memory_.get()}
    , frame_memory_block_{game_memory_->acquire_block(data.frame_memory_capacity_bytes,
                                                      ml::FrameMemoryResource::backing_alignment)}
    , frame_memory_{std::span<std::byte>{frame_memory_block_.data(),
                                         frame_memory_block_.size_bytes()}}
    , query_manager_{entity_tables_, &game_memory_->memory_resource()}
    , overlap_handler_{combat_events_, data.overlap_response}
    , lasers_simulation_{clock_, combat_events_, query_manager_}
    , lasers_phase_{lasers_simulation_}
    , fighters_simulation_{clock_,
                           entity_ledger_,
                           combat_events_,
                           entity_tables_,
                           query_manager_,
                           lasers_simulation_}
    , fighters_phase_{fighters_simulation_}
    , capital_ships_simulation_{entity_ledger_,
                                combat_events_,
                                entity_tables_,
                                query_manager_,
                                fighters_simulation_}
    , capital_ships_phase_{capital_ships_simulation_}
    , turrets_simulation_{clock_,
                          entity_ledger_,
                          combat_events_,
                          entity_tables_,
                          query_manager_,
                          lasers_simulation_}
    , turrets_phase_{turrets_simulation_}
    , spinners_simulation_{clock_, entity_ledger_, entity_tables_, lasers_simulation_}
    , spinners_phase_{spinners_simulation_}
    , mission_manager_{clock_, entity_ledger_, entity_tables_}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     mission_manager_}
    , level_telemetry_manager_{
          clock_, entity_ledger_, lasers_simulation_, *game_memory_, data.telemetry_history} {
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
        ml::FrameScratchScope scratch_scope{frame_memory_};
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
    player_ship_phase_.emplace(player);
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
        player_ship_phase_->begin_play();
    }

    // Start world subsystems
    capital_ships_phase_.begin_play();
    fighters_phase_.begin_play();
    turrets_phase_.begin_play();

    spinners_phase_.begin_play();
    lasers_phase_.begin_play();
}
void LevelSim::initialise_events(CompiledLevelEvents events) {
    auto const player_id{player_ship_simulation_.has_value()
                             ? player_ship_simulation_->unique_entity_id
                             : EntityUniqueId{}};
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

    ++frame_sequence_;
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    query_manager_.reset_frame_collision_events();
    clock_.tick_loop.add_time(dt);

    auto publish_entity_deaths{[&] {
        SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
        capital_ships_phase_.publish_deaths();
        fighters_phase_.publish_deaths();
        turrets_phase_.publish_deaths();
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
            fighters_phase_.commit_orders();
            auto const previous_issued_counts{entity_ledger_.get_issued_counts()};
            event_manager_.execute_tick(clock_.completed_ticks + 1);

            // Commit spawns
            auto const previous_fighter_count{fighters_simulation_.get_num_instances()};
            fighters_phase_.commit_spawns();
            capital_ships_simulation_.fighters_spawned +=
                fighters_simulation_.get_num_instances() - previous_fighter_count;

            lasers_phase_.commit_spawns();

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
            collect_new(capital_ships_simulation_.get_read_view().entities);
            collect_new(fighters_simulation_.get_read_view().entities);
            collect_new(turrets_simulation_.get_read_view().entities);
            // NOLINTEND(ioj-loop-view-accessor-call)

            // Prepare phases and indexes
            if (player_active) {
                player_ship_phase_->prepare_tick(tick_period);
            } else if (player_ship_simulation_) {
                player_ship_simulation_->update_entity_lookup_table();
            }
            capital_ships_phase_.prepare_tick(tick_period);
            fighters_phase_.prepare_tick(tick_period);
            turrets_phase_.prepare_tick(tick_period);
            spinners_phase_.prepare_tick(tick_period);

            refresh_spatial_index();
        }

        /* -------------------------------------------------------------------------------- */
        // Thinking
        /* -------------------------------------------------------------------------------- */
        {
            ml::FrameScratchScope scratch_scope{frame_memory_};
            auto* const scratch_resource{&frame_memory_};

            SANDBOX_PROFILE_SCOPE("Thinking");
            clock_.transition_to(SimulationPhase::Thinking);

            // Resolve dependent reads after every entity type has updated its lookup table.
            mission_manager_.prepare_objectives(scratch_resource);
            capital_ships_simulation_.refresh_fighter_ids(scratch_resource);

#ifndef NDEBUG
            auto const thinking_state{capture_thinking_phase_state()};
            assert(check_phase_invariants(thinking_state));
#endif

            // Run decision phases
            turrets_phase_.think(tick_period, scratch_resource);
            capital_ships_phase_.think(tick_period, scratch_resource);
            fighters_phase_.think(tick_period, scratch_resource);
            if (player_active) {
                player_ship_phase_->think(tick_period);
            }
            spinners_phase_.think(tick_period);

            // Generate fire commands
            if (player_active) {
                player_ship_phase_->generate_fire_commands();
            }
            fighters_phase_.generate_fire_commands(scratch_resource);
            turrets_phase_.generate_fire_commands(scratch_resource);
            spinners_phase_.generate_fire_commands();

            assert(check_phase_invariants(thinking_state));
        }

        /* -------------------------------------------------------------------------------- */
        // Action
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Action");
            clock_.transition_to(SimulationPhase::Action);

            // Simulate projectiles
            {
                ml::FrameScratchScope scratch_scope{frame_memory_};
                lasers_phase_.simulate(tick_period, &frame_memory_);
            }

            // Track player movement
            if (player_active) {
                auto const before{player_ship_simulation_->get_physical_state().transform};
                player_ship_phase_->apply_movement();
                auto const after{player_ship_simulation_->get_physical_state().transform};
                auto const before_location{to_float(before.location)};
                auto const after_location{to_float(after.location)};
                auto const before_rotation{to_float(before.rotator())};
                auto const after_rotation{to_float(after.rotator())};
                if (before_location.X != after_location.X ||
                    before_location.Y != after_location.Y ||
                    before_location.Z != after_location.Z ||
                    before_rotation.pitch != after_rotation.pitch ||
                    before_rotation.yaw != after_rotation.yaw ||
                    before_rotation.roll != after_rotation.roll) {
                    overlap_candidates_.push_back(player_ship_simulation_->unique_entity_id);
                }
            }

            // Move dynamic entities
            {
                ml::FrameScratchScope scratch_scope{frame_memory_};
                auto* const scratch_resource{&frame_memory_};
                fighters_phase_.apply_movement(scratch_resource);
                spinners_phase_.apply_movement(scratch_resource);
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
                ml::FrameScratchScope scratch_scope{frame_memory_};
                auto const overlaps{
                    // Detection creates a new scratch-backed result for this tick.
                    // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
                    query_manager_.detect_overlaps(overlap_candidates_, &frame_memory_)};
                overlap_handler_.handle(overlaps);
            }
        }

        /* -------------------------------------------------------------------------------- */
        // Resolution
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Resolution");
            clock_.transition_to(SimulationPhase::Resolution);

            {
                ml::FrameScratchScope scratch_scope{frame_memory_};

                // Prepare combat events
                combat_events_.prepare(entity_tables_.lookups, &frame_memory_);

                // Resolve damage
                if (player_active) {
                    player_ship_phase_->resolve_damage_events();
                }
                capital_ships_phase_.resolve_damage_events();
                fighters_phase_.resolve_damage_events(&frame_memory_);
                turrets_phase_.resolve_damage_events();
                capital_ships_phase_.resolve_fighters_of_dying_capitals();

                // Publish deaths
                publish_entity_deaths();
            }

            // Evaluate final health and deaths before invalidating row mappings.
            {
                ml::FrameScratchScope scratch_scope{frame_memory_};
                mission_manager_.mission_tick(&frame_memory_);
            }

            // Clean up entities and indexes
            {
                ml::FrameScratchScope scratch_scope{frame_memory_};

                SANDBOX_PROFILE_SCOPE("ResolutionCommit");
                clock_.transition_to(SimulationPhase::ResolutionCommit);

                auto const removed_entities{
                    !capital_ships_simulation_.local_indices_to_remove.empty() ||
                    !fighters_simulation_.local_indices_to_remove.empty() ||
                    !turrets_simulation_.local_indices_to_remove.empty() ||
                    (player_active && !player_ship_simulation_->is_alive())};

                // Apply the same removals to fixed health ranges and their owning rows.
                capital_ships_phase_.remove_components();
                fighters_phase_.remove_components();
                turrets_phase_.remove_components();

                // Retire dead identities and compact owning storage.
                capital_ships_phase_.remove_entities();
                fighters_phase_.remove_entities();
                turrets_phase_.remove_entities();
                lasers_phase_.cleanup_entities();
                capital_ships_simulation_.refresh_fighter_ids(&frame_memory_);

                // Keep geometry-only queries valid between ticks after removing dead owners.
                if (removed_entities) {
                    refresh_spatial_index();
                }
            }

            // Finish tick and reset frame state
            capital_ships_phase_.finish_action();
            fighters_phase_.finish_action();
            turrets_phase_.finish_action();
            spinners_phase_.finish_action();
            lasers_phase_.finish_action();
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
    std::optional<PlayerSpatialData> player;
    if (player_ship_simulation_) {
        auto const& simulation{*player_ship_simulation_};
        auto const& physical{simulation.get_physical_state()};
        player = PlayerSpatialData{simulation.unique_entity_id,
                                   to_float(physical.transform.location),
                                   to_float(physical.velocity),
                                   to_quaternion(to_float(physical.transform.rotator())),
                                   simulation.get_health().health};
    }
    query_manager_.refresh_spatial_index(capital_ships_simulation_.get_read_view(),
                                         fighters_simulation_.get_read_view(),
                                         turrets_simulation_.get_read_view(),
                                         spinners_simulation_.get_read_view(),
                                         player);
}

auto LevelReadAccess::get_capitals() const -> CapitalReadView {
    return level_.get_capital_ships().get_read_view();
}
auto LevelReadAccess::get_fighters() const -> FighterReadView {
    return level_.get_fighters().get_read_view();
}
auto LevelReadAccess::get_turrets() const -> TurretReadView {
    return level_.get_turrets().get_read_view();
}
auto LevelReadAccess::get_spinners() const -> SpinnerReadView {
    return level_.get_spinners().get_read_view();
}
auto LevelReadAccess::get_lasers() const -> LaserReadView {
    return level_.get_lasers().get_read_view();
}
auto LevelReadAccess::get_player() const -> std::optional<PlayerReadView> {
    auto const* player{level_.get_player_ship_simulation()};
    return player ? std::optional{player->get_read_view()} : std::nullopt;
}
auto LevelReadAccess::get_clock() const -> SimClock const& {
    return level_.get_clock();
}
auto LevelReadAccess::frame_sequence() const -> std::uint64_t {
    return level_.frame_sequence_;
}
auto LevelReadAccess::interpolation_alpha() const -> double {
    auto const& clock{get_clock()};
    return std::clamp(clock.tick_loop.accumulator / clock.get_tick_period(), 0.0, 1.0);
}
} // namespace ioj::sim

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/level_sim.h>
#include <ioj/sim/profiling.h>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <span>
#include <thread>
#include <vector>

namespace ioj::sim {

namespace level_simulation {
/* **************************************** */
// Participating teams
/* **************************************** */
static void finalise_participating_teams(LevelSimInitData& data) {
    std::array<std::uint8_t, static_cast<std::size_t>(Team::COUNT)> included{};
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

[[nodiscard]] static auto health_capacity(LevelSimInitData const& data) -> std::size_t {
    auto capacity{data.player.has_value() ? std::size_t{1} : std::size_t{0}};
    capacity += static_cast<std::size_t>(std::max(0, data.fighters.max_live_fighters));
    capacity += data.level_events.initial_spawns.capital_spawns.num();
    capacity += data.level_events.initial_spawns.turret_spawns.num();
    capacity += data.level_events.schedule.capital_spawns.num();
    capacity += data.level_events.schedule.turret_spawns.num();
    return capacity;
}

}

/* **************************************** */
// Construction and lifecycle
/* **************************************** */
LevelSim::LevelSim(LevelSimInitData data)
    : local_game_memory_{data.game_memory == nullptr ? std::make_unique<GameMemory>() : nullptr}
    , game_memory_{data.game_memory != nullptr ? data.game_memory : local_game_memory_.get()}
    , frame_memory_{data.frame_memory_capacity_bytes}
    , query_manager_{agent_accessor_}
    , overlap_handler_{combat_events_, agent_accessor_, data.overlap_response}
    , lasers_simulation_{clock_, combat_events_, query_manager_, frame_memory_}
    , lasers_phase_{lasers_simulation_}
    , fighters_simulation_{clock_,
                           entity_ledger_,
                           combat_events_,
                           entity_tables_.health,
                           agent_accessor_,
                           query_manager_,
                           lasers_simulation_,
                           frame_memory_}
    , fighters_phase_{fighters_simulation_}
    , capital_ships_simulation_{entity_ledger_,
                                combat_events_,
                                entity_tables_.health,
                                agent_accessor_,
                                query_manager_,
                                fighters_simulation_,
                                frame_memory_}
    , capital_ships_phase_{capital_ships_simulation_}
    , turrets_simulation_{clock_,
                          entity_ledger_,
                          combat_events_,
                          entity_tables_.health,
                          agent_accessor_,
                          query_manager_,
                          lasers_simulation_,
                          frame_memory_}
    , turrets_phase_{turrets_simulation_}
    , spinners_simulation_{clock_, entity_ledger_, lasers_simulation_, frame_memory_}
    , spinners_phase_{spinners_simulation_}
    , mission_manager_{clock_, entity_ledger_, agent_accessor_}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     mission_manager_}
    , level_telemetry_manager_{
          clock_, entity_ledger_, lasers_simulation_, *game_memory_, data.telemetry_history} {
    // Initialise timing and metadata
    clock_.initialise(data.clock_settings);
    entity_tables_.health.reserve(level_simulation::health_capacity(data));
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
    rebuild_agent_indexes();
    query_manager_.update({}, clock_.get_completed_ticks());

    // Start telemetry and mission
    initialise_telemetry();
    event_manager_.configure_mission();
    mission_manager_.begin_play();

    state_ = OrchestratorState::Paused;
    clock_.phase = SimulationPhase::Idle;
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
                                                 entity_tables_.health,
                                                 query_manager_,
                                                 lasers_simulation_)};
    player_ship_phase_.emplace(player);
    player_ship_commands_.emplace(player);

    player.configure(spawn);
}
void LevelSim::initialise_spatial_queries(LevelSimInitData& data) {
    // Configure collision queries and buffers
    query_manager_.initialise(data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency())));

    // Install static bounds
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(
        std::move(data.static_bounds));
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
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(std::move(bounds));
}
auto LevelSim::add_static_collision_aabb(Vector3f const min_point, Vector3f const max_point)
    -> std::int32_t {
    return query_manager_.get_collision_system().get_uniform_grid().add_static_aabb(min_point,
                                                                                    max_point);
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
    query_manager_.get_collision_system().reset_frame_events();
    clock_.tick_loop.add_time(dt);

    auto publish_entity_deaths{[&] {
        SANDBOX_PROFILE_SCOPE("LevelSim::advance::publish_entity_deaths");
        capital_ships_phase_.publish_deaths();
        fighters_phase_.publish_deaths();
        turrets_phase_.publish_deaths();
    }};

    while (clock_.tick_loop.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_loop.tick_period)};
        auto const player_active{player_ship_simulation_.has_value() &&
                                 player_ship_simulation_->is_alive()};

        /* -------------------------------------------------------------------------------- */
        // Preparation
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Preparation");
            clock_.phase = SimulationPhase::Preparation;

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
            collision_dirty_entities_.clear();
            auto collect_new = [&](auto const data) {
                auto const count{data.entity_ids.size()};
                for (std::size_t index{}; index < count; ++index) {
                    auto const id{data.entity_ids[index]};
                    if (id.index() >=
                        entity_identity_offset(id.entity_type(),
                                               previous_issued_counts[id.entity_type()])) {
                        collision_dirty_entities_.push_back(id);
                    }
                }
            };

            collect_new(capital_ships_simulation_.get_read_view().entities);
            collect_new(fighters_simulation_.get_read_view().entities);
            collect_new(turrets_simulation_.get_read_view().entities);

            // Prepare phases and indexes
            if (player_active) {
                player_ship_phase_->prepare_tick(tick_period);
            }
            capital_ships_phase_.prepare_tick(tick_period);
            fighters_phase_.prepare_tick(tick_period);
            turrets_phase_.prepare_tick(tick_period);
            spinners_phase_.prepare_tick(tick_period);

            rebuild_agent_indexes();
            mission_manager_.prepare_objectives();
            capital_ships_simulation_.refresh_fighter_ids();
            query_manager_.get_collision_system().refresh_queries();
        }
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Thinking
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Thinking");
            clock_.phase = SimulationPhase::Thinking;

            // Run decision phases
            turrets_phase_.think(tick_period);
            capital_ships_phase_.think(tick_period);
            fighters_phase_.think(tick_period);
            if (player_active) {
                player_ship_phase_->think(tick_period);
            }
            spinners_phase_.think(tick_period);

            // Generate fire commands
            if (player_active) {
                player_ship_phase_->generate_fire_commands();
            }
            fighters_phase_.generate_fire_commands();
            turrets_phase_.generate_fire_commands();
            spinners_phase_.generate_fire_commands();
        }
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Action
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Action");
            clock_.phase = SimulationPhase::Action;

            // Simulate projectiles
            lasers_phase_.simulate(tick_period);
            frame_memory_.reclaim();

            // Track player movement
            if (player_active) {
                auto const before{player_ship_simulation_->get_movement_state().transform};
                player_ship_phase_->apply_movement();
                auto const after{player_ship_simulation_->get_movement_state().transform};
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
                    collision_dirty_entities_.push_back(player_ship_simulation_->unique_entity_id);
                }
            }

            // Move dynamic entities
            fighters_phase_.apply_movement();
            spinners_phase_.apply_movement();
            frame_memory_.reclaim();

            {
                SANDBOX_PROFILE_SCOPE("Update collision overlaps");

                auto const moved{fighters_simulation_.get_collision_dirty_entities()};
                collision_dirty_entities_.insert(
                    collision_dirty_entities_.end(), moved.begin(), moved.end());
                std::ranges::sort(collision_dirty_entities_);
                auto const duplicates{std::ranges::unique(collision_dirty_entities_)};
                collision_dirty_entities_.erase(duplicates.begin(), duplicates.end());
                auto const overlaps{query_manager_.get_collision_system().update(
                    collision_dirty_entities_, clock_.completed_ticks + 1)};
                overlap_handler_.handle(overlaps);
            }
        }

        /* -------------------------------------------------------------------------------- */
        // Resolution
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Resolution");
            clock_.phase = SimulationPhase::Resolution;

            // Prepare combat events
            combat_events_.prepare(agent_indexes_, frame_memory_);

            // Resolve damage
            if (player_active) {
                player_ship_phase_->resolve_damage_events();
            }
            capital_ships_phase_.resolve_damage_events();
            fighters_phase_.resolve_damage_events();
            turrets_phase_.resolve_damage_events();
            capital_ships_phase_.resolve_fighters_of_dying_capitals();

            // Publish deaths
            publish_entity_deaths();
            frame_memory_.reclaim();

            // Clean up entities and indexes
            {
                SANDBOX_PROFILE_SCOPE("ResolutionCommit");
                clock_.phase = SimulationPhase::ResolutionCommit;

                capital_ships_phase_.cleanup_entities();
                fighters_phase_.cleanup_entities();
                turrets_phase_.cleanup_entities();
                lasers_phase_.cleanup_entities();
                rebuild_agent_indexes();
                capital_ships_simulation_.refresh_fighter_ids();
            }

            clock_.phase = SimulationPhase::Resolution;

            // Finish tick and reset frame state
            mission_manager_.mission_tick();
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
        clock_.phase = SimulationPhase::Idle;

        if (state_ != OrchestratorState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Read views
/* **************************************** */

void LevelSim::rebuild_agent_indexes() {
    SANDBOX_PROFILE_SCOPE("LevelSim::rebuild_agent_indexes");
    assert(clock_.permits_structural_mutation());

    // Capture entity views
    auto const capitals{capital_ships_simulation_.get_read_view().entities};
    auto const fighters{fighters_simulation_.get_read_view().entities};
    auto const turrets{turrets_simulation_.get_read_view().entities};
    auto const spinners{spinners_simulation_.get_read_view().entities};

    // Bind entity indexes
    agent_indexes_.bind(EntityType::CapitalShip, capitals.entity_ids);
    agent_indexes_.bind(EntityType::Fighter, fighters.entity_ids);
    agent_indexes_.bind(EntityType::Turret, turrets.entity_ids);
    agent_indexes_.bind(EntityType::TubeSpinner, spinners.entity_ids);

    // Bind player index
    PlayerAgentView player_view{};
    if (player_ship_simulation_ && player_ship_simulation_->is_alive()) {
        auto const& player{*player_ship_simulation_};
        agent_indexes_.bind(EntityType::PlayerShip, {&player.unique_entity_id, 1});
        player_view = {&player.get_movement_state().transform,
                       &player.get_movement_state().velocity,
                       player.get_health_index(),
                       &player.team,
                       player.unique_entity_id};
    } else {
        if (player_ship_simulation_) {
            agent_indexes_.retire(player_ship_simulation_->unique_entity_id);
        }
    }

    // Publish agent views
    agent_accessor_.bind(capitals, fighters, turrets, spinners, player_view);
}

auto LevelSim::get_read_view() const -> LevelReadView {
    return {frame_sequence_,
            &clock_,
            capital_ships_simulation_.get_read_view(),
            fighters_simulation_.get_read_view(),
            turrets_simulation_.get_read_view(),
            spinners_simulation_.get_read_view(),
            lasers_simulation_.get_read_view(),
            player_ship_simulation_.has_value()
                ? std::optional<PlayerReadView>{player_ship_simulation_->get_read_view()}
                : std::nullopt,
            &mission_manager_};
}
} // namespace ioj::sim

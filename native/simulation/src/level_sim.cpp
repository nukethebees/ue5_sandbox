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
        for (auto const team : data.level_events.schedule.capital_spawns.get_const_view().teams()) {
            include(team);
        }
        for (auto const team : data.level_events.schedule.turret_spawns.get_const_view().teams()) {
            include(team);
        }
    }

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
    , frame_memory_{data.frame_memory_capacity_bytes}
    , query_manager_{entity_registry_}
    , overlap_handler_{entity_registry_, data.overlap_response}
    , lasers_simulation_{clock_, entity_registry_, query_manager_, frame_memory_}
    , lasers_phase_{lasers_simulation_}
    , fighters_simulation_{clock_,
                           entity_registry_,
                           query_manager_,
                           lasers_simulation_,
                           frame_memory_}
    , fighters_phase_{fighters_simulation_}
    , capital_ships_simulation_{entity_registry_,
                                query_manager_,
                                fighters_simulation_,
                                frame_memory_}
    , capital_ships_phase_{capital_ships_simulation_}
    , turrets_simulation_{clock_,
                          entity_registry_,
                          query_manager_,
                          lasers_simulation_,
                          frame_memory_}
    , turrets_phase_{turrets_simulation_}
    , spinners_simulation_{clock_, entity_registry_, lasers_simulation_, frame_memory_}
    , spinners_phase_{spinners_simulation_}
    , mission_manager_{clock_, entity_registry_}
    , event_manager_{capital_ships_simulation_,
                     turrets_simulation_,
                     spinners_simulation_,
                     mission_manager_}
    , level_telemetry_manager_{
          clock_, entity_registry_, lasers_simulation_, *game_memory_, data.telemetry_history} {
    clock_.initialise(data.clock_settings);
    telemetry_metadata_ = std::move(data.telemetry_metadata);
    level_simulation::finalise_participating_teams(data);

    initialise_spatial_queries(data);
    configure_subsystems(data);
    begin_subsystems();

    initialise_events(std::move(data.level_events));
    event_manager_.execute_tick(0);
    validate_entity_handles();
}
void LevelSim::finish_initialisation() {
    assert(state_ == OrchestratorState::Uninitialised);

    entity_registry_.commit_updates();
    query_manager_.update(clock_.get_completed_ticks());
    entity_registry_.end_tick();

    initialise_telemetry();

    event_manager_.configure_mission();
    mission_manager_.begin_play();

    state_ = OrchestratorState::Paused;
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
    set_fighter_diagnostics_enabled(data.fighter_diagnostics_enabled);
    if (data.player.has_value()) {
        configure_player(data.player.value());
    }

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
    auto& player{player_ship_simulation_.emplace(
        clock_, entity_registry_, query_manager_, lasers_simulation_)};
    player_ship_phase_.emplace(player);
    player_ship_commands_.emplace(player);

    player.configure(spawn);
}
void LevelSim::initialise_spatial_queries(LevelSimInitData& data) {
    query_manager_.initialise(data.grid_dimensions, data.cell_size, data.entity_bounds);
    query_manager_.reserve_thread_buffers(
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency())));
    query_manager_.get_collision_system().get_uniform_grid().set_static_aabbs(
        std::move(data.static_bounds));
}
void LevelSim::begin_subsystems() {
    if (player_ship_simulation_.has_value()) {
        player_ship_phase_->begin_play();
    }

    capital_ships_phase_.begin_play();
    fighters_phase_.begin_play();
    turrets_phase_.begin_play();

    spinners_phase_.begin_play();
    lasers_phase_.begin_play();
}
void LevelSim::initialise_events(CompiledLevelEvents events) {
    auto const player_handle{player_ship_simulation_.has_value()
                                 ? player_ship_simulation_->registry_handle
                                 : RegistryEntityHandle{}};
    event_manager_.initialise(std::move(events), player_handle);
}
void LevelSim::validate_entity_handles() const {
    if (player_ship_simulation_.has_value()) {
        assert(entity_registry_.is_valid_handle(player_ship_simulation_->registry_handle));
    }
    capital_ships_simulation_.validate_entity_handles();
    turrets_simulation_.validate_entity_handles();
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
    SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance");

    if (state_ != OrchestratorState::Running) {
        return;
    }

    ++frame_sequence_;
    capital_ships_simulation_.reset_frame_output();
    turrets_simulation_.reset_frame_output();
    lasers_simulation_.reset_frame_output();
    query_manager_.get_collision_system().reset_frame_events();
    clock_.tick_loop.add_time(dt);

    while (clock_.tick_loop.try_tick()) {
        auto const tick_period{static_cast<float>(clock_.tick_loop.tick_period)};
        auto const player_active{player_ship_simulation_.has_value() &&
                                 player_ship_simulation_->health.is_alive()};
        auto publish_entity_state = [&] {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::advance::publish_entity_state");
            if (player_ship_simulation_.has_value() && player_ship_simulation_->health.is_alive()) {
                player_ship_phase_->update_entity_registry();
            }
            capital_ships_phase_.update_entity_registry();
            fighters_phase_.update_entity_registry();
            turrets_phase_.update_entity_registry();
            entity_registry_.commit_updates();
        };

        /* -------------------------------------------------------------------------------- */
        // Preparation
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Preparation");
            entity_registry_.begin_tick();
            if (player_active) {
                player_ship_phase_->prepare_tick(tick_period);
            }
            capital_ships_phase_.prepare_tick(tick_period);
            fighters_phase_.prepare_tick(tick_period);
            turrets_phase_.prepare_tick(tick_period);
            spinners_phase_.prepare_tick(tick_period);
        }
        frame_memory_.reclaim();

        /* -------------------------------------------------------------------------------- */
        // Thinking
        /* -------------------------------------------------------------------------------- */
        {
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Thinking");
            turrets_phase_.think(tick_period);
            capital_ships_phase_.think(tick_period);
            fighters_phase_.think(tick_period);
            if (player_active) {
                player_ship_phase_->think(tick_period);
            }
            spinners_phase_.think(tick_period);

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
            SANDBOX_PROFILE_SCOPE("Sandbox::LevelSim::Action");

            // Existing projectiles retain pre-movement collision geometry.
            lasers_phase_.simulate(tick_period);
            frame_memory_.reclaim();

            if (player_active) {
                player_ship_phase_->apply_movement();
            }
            fighters_phase_.apply_movement();
            spinners_phase_.apply_movement();
            frame_memory_.reclaim();

            event_manager_.execute_tick(clock_.completed_ticks + 1);
            fighters_phase_.commit_spawns();
            capital_ships_phase_.execute_fighter_self_destruct_requests();
            lasers_phase_.commit_spawns();
            frame_memory_.reclaim();

            // Collision observes moved and newly created entities, before resolved deaths.
            publish_entity_state();
            collision::DetectedOverlapsView overlaps;
            {
                auto const authored_spawns{event_manager_.get_spawned_handles()};
                if (authored_spawns.empty()) {
                    overlaps = query_manager_.update(clock_.completed_ticks + 1);
                } else {
                    auto const moved{entity_registry_.get_moved_entities_this_tick()};
                    collision_dirty_entities_.assign(moved.begin(), moved.end());
                    collision_dirty_entities_.insert(collision_dirty_entities_.end(),
                                                     authored_spawns.begin(),
                                                     authored_spawns.end());
                    // Carrier fighters remain queryable but retain their unmoved launch boundary.
                    std::ranges::sort(collision_dirty_entities_);
                    auto const duplicates{std::ranges::unique(collision_dirty_entities_)};
                    collision_dirty_entities_.erase(duplicates.begin(), duplicates.end());
                    overlaps = query_manager_.get_collision_system().update(
                        collision_dirty_entities_, clock_.completed_ticks + 1);
                }
                overlap_handler_.handle(overlaps);
            }

            if (player_active) {
                player_ship_phase_->resolve_damage_events();
            }
            capital_ships_phase_.resolve_damage_events();
            fighters_phase_.resolve_damage_events();
            turrets_phase_.resolve_damage_events();
            capital_ships_phase_.resolve_fighters_of_dying_capitals();

            // Queue final rows before compaction; include all capital-death consequences.
            publish_entity_state();
            auto const queries_need_refresh{
                !entity_registry_.get_dead_entities_this_frame().empty()};
            capital_ships_phase_.cleanup_entities();
            fighters_phase_.cleanup_entities();
            turrets_phase_.cleanup_entities();
            if (queries_need_refresh) {
                query_manager_.get_collision_system().refresh_queries();
            }
            frame_memory_.reclaim();

            mission_manager_.mission_tick();
            capital_ships_phase_.finish_action();
            fighters_phase_.finish_action();
            turrets_phase_.finish_action();
            spinners_phase_.finish_action();
            lasers_phase_.finish_action();
            entity_registry_.end_tick();

            ++clock_.completed_ticks;
            level_telemetry_manager_.tick();
            frame_memory_.reset();
        }
        profiling::mark_frame("Simulation");

        if (state_ != OrchestratorState::Running) {
            break;
        }
    }
}

/* **************************************** */
// Read views
/* **************************************** */

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
            &entity_registry_,
            &mission_manager_};
}
} // namespace ioj::sim

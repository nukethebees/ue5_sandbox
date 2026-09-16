#include "ioj/sim/fighters/sim.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <ioj/sim/deterministic_bias.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/vector_operations.h>
#include <limits>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/core/vector_math.h>
#include <sandbox/core/vector_normalization.h>
#include <span>
#include <utility>
#include <vector>

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/fighter_diagnostics.h>
#include <ioj/sim/frame_laser_spawn_requests.h>
#include <ioj/sim/frame_trace_hits.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/laser_source.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_array.h>
#include <sandbox/core/generated/array_math_kernels.h>
#include <sandbox/core/projectile_intercept.h>
#include <sandbox/core/vector_normalization.h>

namespace ioj::sim::fighters {
namespace {
void copy_vectors(Vectors3fView const destination, Vectors3fConstView const source) {
    std::ranges::copy(source.xs_span(), destination.xs);
    std::ranges::copy(source.ys_span(), destination.ys);
    std::ranges::copy(source.zs_span(), destination.zs);
}
void copy_rotators(Rotators3fView const destination, Rotators3fConstView const source) {
    std::ranges::copy(source.pitches, destination.pitches.begin());
    std::ranges::copy(source.yaws, destination.yaws.begin());
    std::ranges::copy(source.rolls, destination.rolls.begin());
}
void append_fighter_spawns(SingleAllocationFighterSpawnQueue& destination,
                           FighterSpawnQueueConstView const source) {
    auto const old_count{destination.num()};
    destination.add_uninitialised(source.num());
    auto const output{destination.get_view().columns().get_view(old_count, source.num())};
    copy_vectors(output.locations, source.locations);
    copy_rotators(output.rotations, source.rotations);
    std::ranges::copy(source.teams, output.teams.begin());
    std::ranges::copy(source.parents, output.parents.begin());
    std::ranges::copy(source.targets, output.targets.begin());
}
} // namespace

namespace firing_detail {
struct FirePointCandidate {
    Vector3f location;
    Vector3f trace_start;
    Vector3f trace_end;
};

struct FirePointAngleOffset {
    float yaw;
    float pitch;
};

inline constexpr std::array<FirePointAngleOffset, 16> angle_offsets{{
    {0.f, 0.f},
    {45.f, 0.f},
    {-45.f, 0.f},
    {90.f, 0.f},
    {-90.f, 0.f},
    {135.f, 0.f},
    {-135.f, 0.f},
    {180.f, 0.f},
    {0.f, 35.f},
    {90.f, 35.f},
    {180.f, 35.f},
    {-90.f, 35.f},
    {45.f, -35.f},
    {135.f, -35.f},
    {-135.f, -35.f},
    {-45.f, -35.f},
}};

auto make_fire_point_candidate(Vector3f const target_location,
                               Vector3f const reference_location,
                               float const fire_point_distance,
                               float const trace_end_offset,
                               float const desired_attack_distance,
                               std::uint32_t const integral_bias,
                               float const float_bias,
                               std::uint32_t const candidate_order) noexcept -> FirePointCandidate {
    auto const base_direction{ml::native_math::safe_normal(reference_location - target_location)};
    auto rotation{direction_to_rotation(base_direction)};
    auto const first_index{integral_bias % angle_offsets.size()};
    auto const offset{angle_offsets[(first_index + candidate_order) % angle_offsets.size()]};
    rotation.yaw += float_bias * 360.f + offset.yaw;
    rotation.pitch += offset.pitch;
    auto const candidate_location{target_location +
                                  forward_direction(rotation) * desired_attack_distance};

    std::array<double, 3> const target{target_location.X, target_location.Y, target_location.Z};
    std::array<double, 3> const candidate{
        candidate_location.X, candidate_location.Y, candidate_location.Z};
    std::array<double, 3> aim{
        target[0] - candidate[0], target[1] - candidate[1], target[2] - candidate[2]};
    ml::native_math::safe_normal_components(aim[0], aim[1], aim[2]);
    std::array<double, 3> const start{candidate[0] + aim[0] * fire_point_distance,
                                      candidate[1] + aim[1] * fire_point_distance,
                                      candidate[2] + aim[2] * fire_point_distance};
    std::array<double, 3> direction{
        target[0] - start[0], target[1] - start[1], target[2] - start[2]};
    ml::native_math::safe_normal_components(direction[0], direction[1], direction[2]);
    return {candidate_location,
            HMM_V3(static_cast<float>(start[0]),
                   static_cast<float>(start[1]),
                   static_cast<float>(start[2])),
            HMM_V3(static_cast<float>(target[0] - direction[0] * trace_end_offset),
                   static_cast<float>(target[1] - direction[1] * trace_end_offset),
                   static_cast<float>(target[2] - direction[2] * trace_end_offset))};
}
}
namespace diagnostic_detail {
auto vector_string(Vector3f const value) -> std::string {
    return std::format("({}, {}, {})", value.X, value.Y, value.Z);
}
}

/* **************************************** */
// Navigation helpers
/* **************************************** */
auto Sim::get_navigation_tick_period(NavigationRiskTier const tier) const -> std::int16_t {
    auto const index{static_cast<std::int32_t>(tier)};
    assert(index >= 0 && static_cast<std::size_t>(index) < navigation_tick_periods.size());
    return navigation_tick_periods[index];
}
void Sim::reset_navigation_state(std::int32_t const fighter_index,
                                 NavigationRiskTier const initial_tier) {
    auto const data{entity_buffers.current().get_view().columns()};
    assert(fighter_index >= 0 && fighter_index < data.num());
    data.separation_steering.set(fighter_index, HMM_V3(0.f, 0.f, 0.f));
    data.navigation_risk_tiers[fighter_index] = static_cast<std::uint8_t>(initial_tier);
    data.navigation_lower_risk_scan_counts[fighter_index] = 0;
    data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
    data.avoidance_clear_scan_counts[fighter_index] = 0;
    data.navigation_update_countdowns_periods[fighter_index] =
        get_navigation_tick_period(initial_tier);
    data.navigation_update_countdowns_remaining_ticks[fighter_index] = 0;
}

/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(FighterSimConfig const& new_config,
                     FighterLevelData const level_data) noexcept {
    config = new_config;
    collision_radius_ = level_data.collision_radius;
    fire_point_distance_ = level_data.fire_point_distance;

    for (auto& is_participant : participant_mask) {
        is_participant = 0;
    }
    for (auto const team : level_data.participating_teams) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < participant_mask.size()) {
            participant_mask[team_index] = 1;
        }
    }

    assert(std::in_range<std::int32_t>(level_data.participating_teams.size()));
    auto const participant_count{static_cast<std::int32_t>(level_data.participating_teams.size())};
    per_team_limit =
        participant_count == 0 ? 0 : std::max(0, config.max_live_fighters) / participant_count;
}
Sim::Sim(SimClock const& clock,
         EntityRegistry& in_entity_registry,
         AgentAccessor const& agents,
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_laser_simulation,
         std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , agents_{agents}
    , spatial_query_manager{in_spatial_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::begin_play");
    profiling::plot("Sandbox/FighterCount", 0);
    profiling::plot("Sandbox/FightersAvoiding", 0);
    profiling::plot("Sandbox/FighterNavigationTraces", 0);
    profiling::plot("Sandbox/FightersSeparating", 0);
    profiling::plot("Sandbox/FighterSeparationQueries", 0);
    profiling::plot("Sandbox/FighterSeparationCandidates", 0);
    profiling::plot("Sandbox/FighterDenseDirectionSelections", 0);
    profiling::plot("Sandbox/FighterSteeringMemory", 0);
    profiling::plot("Sandbox/FighterNavigationClear", 0);
    profiling::plot("Sandbox/FighterNavigationNearby", 0);
    profiling::plot("Sandbox/FighterNavigationActive", 0);
    profiling::plot("Sandbox/FighterNavigationImmediate", 0);
    awareness_cleaner_ = 0;
    reposition_cleaner_ = 0;
    attack_cleaner_ = 0;
    assert(collision_radius_ > 0.f);
    assert(fire_point_distance_ >= 0.f);

    auto const awareness_scan_tick_period{
        simulation_clock.frequency_to_tick_period(config.awareness_scan_frequency)};
    assert(awareness_scan_tick_period >= 0 &&
           std::in_range<std::int8_t>(awareness_scan_tick_period));
    awareness_restart_ticks_ = static_cast<std::int8_t>(awareness_scan_tick_period);

    auto const attack_reposition_tick_period{
        simulation_clock.frequency_to_tick_period(config.attack_reposition_frequency)};
    assert(attack_reposition_tick_period >= 0 &&
           std::in_range<std::int16_t>(attack_reposition_tick_period));
    reposition_restart_ticks_ = static_cast<std::int16_t>(attack_reposition_tick_period);

    std::array<float, static_cast<std::int32_t>(NavigationRiskTier::Count)> const frequencies{
        config.avoidance_clear_update_frequency,
        config.avoidance_update_frequency,
        config.avoidance_active_update_frequency,
        config.avoidance_immediate_update_frequency,
    };
    for (std::int32_t i{}; static_cast<std::size_t>(i) < frequencies.size(); ++i) {
        auto const period{simulation_clock.frequency_to_tick_period(frequencies[i])};
        assert(period > 0 && std::in_range<std::int16_t>(period));
        navigation_tick_periods[i] = static_cast<std::int16_t>(period);
    }
    auto const clear_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Clear) *
                           simulation_clock.get_tick_period())};
    minimum_navigation_lookahead_time = clear_update_interval * 1.25f;

    auto const fire_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(fire_cooldown_tick_period >= 0 &&
           std::in_range<std::int16_t>(fire_cooldown_tick_period));
    attack_restart_ticks_ = static_cast<std::int16_t>(fire_cooldown_tick_period);

    auto const attack_retry_cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.attack_retry_cooldown)};
    assert(attack_retry_cooldown_tick_period >= 0 &&
           std::in_range<std::int16_t>(attack_retry_cooldown_tick_period));
    attack_retry_cooldown_tick_value = static_cast<std::int16_t>(attack_retry_cooldown_tick_period);

    assert(config.attack_distance_band.values_are_valid());
}
void Sim::prepare_tick(float const dt) {
    movement_tick_period_ = dt;
    if (!tasks_are_contiguous()) {
        refresh_layout();
    }
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::prepare_tick");

    auto const data{entity_buffers.current().get_view().columns()};
    ml::tick_countdowns<std::int8_t>(data.awareness_scan_countdowns, awareness_cleaner_, 64);
    ml::tick_countdowns<std::int16_t>(
        data.attack_reposition_countdowns, reposition_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(data.navigation_update_countdowns_remaining_ticks);
    clear_tick_buffers();

    for (auto& capacity : remaining_team_capacity) {
        capacity = per_team_limit;
    }
    for (auto const team : data.teams) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < participant_mask.size() &&
            participant_mask[team_index] != 0) {
            remaining_team_capacity[team_index] =
                std::max(0, remaining_team_capacity[team_index] - 1);
        } else {
            ml::log_error(
                std::format("Live fighter has invalid or non-participating team {}", team_index));
        }
    }
    ml::tick_countdowns<std::int16_t>(data.attack_cooldowns, attack_cleaner_, 16384);
}
void Sim::think(float const dt) {
    refresh_target_data();
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::think");

    auto const data{entity_buffers.current().get_view().columns()};
    auto const awareness_radius{config.awareness_radius};
    auto const attack_engagement_threshold{config.attack_engagement_threshold};
    auto const attack_engagement_threshold_sq{attack_engagement_threshold *
                                              attack_engagement_threshold};
    auto const n{data.num()};
    std::array<RegistryEntityHandle, 128> nearby_entities;
    auto const dot_threshold{config.minimum_opportunistic_intercept_deviation_dot_product};

    {
        SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::awareness_scan");
        for (std::int32_t i{0}; i < n; ++i) {
            if (!ml::TickCountdownView<std::int8_t>{data.awareness_scan_countdowns,
                                                    awareness_restart_ticks_}
                     .try_consume(i)) {
                continue;
            }

            auto const fighter_location{data.locations[i]};
            auto const target_handle{data.target_handles[i]};
            if (agents_.read_alive(entity_registry.get_current_id(target_handle)) &&
                data.target_distance_sq[i] <= attack_engagement_threshold_sq) {
                continue;
            }

            auto const n_nearby_entities{spatial_query_manager.collect_non_team_entities_in_range(
                fighter_location, data.teams[i], awareness_radius, nearby_entities)};
            auto const aim_direction{data.aim_directions[i]};
            RegistryEntityHandle selected_target{};
            for (std::int32_t nearby_index{}; nearby_index < n_nearby_entities; ++nearby_index) {
                auto const candidate{nearby_entities[nearby_index]};
                auto const state{agents_.read_alive(entity_registry.get_current_id(candidate))};
                if (!state) {
                    continue;
                }
                auto const direction{
                    ml::native_math::safe_normal(state->location - fighter_location, 1.e-8f)};
                if (HMM_DotV3(aim_direction, direction) > dot_threshold) {
                    selected_target = candidate;
                    break;
                }
            }

            if (!selected_target.is_null()) {
                data.target_handles[i] = selected_target;
            }
        }
    }
    refresh_target_data();
    plan_movement(dt);
}
void Sim::plan_movement(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::plan_movement");

    auto const d_turn{std::min(1.f, config.turn_speed_unitless * dt)};
    auto const data{entity_buffers.current().get_view().columns()};
    if (data.num() < 1) {
        return;
    }

    copy_vectors(data.planned_aim_directions, data.aim_directions.get_const_view());

    auto const& move_view{get_task_view(Task::MoveToDestination)};
    auto const& attack_view{get_task_view(Task::Attack)};
    auto const do_move{move_view.num() > 0};
    auto const n_attack{attack_view.num()};
    auto const do_attack{n_attack > 0};
    auto const laser_max_distance{config.laser.max_distance};
    auto const& attack_distance_band{config.attack_distance_band};
    auto const desired_attack_distance{laser_max_distance * attack_distance_band.desired_ratio};
    auto const inner_attack_distance{laser_max_distance * attack_distance_band.minimum_ratio};
    auto const outer_attack_distance{laser_max_distance * attack_distance_band.maximum_ratio};

    if (do_attack) {
        auto const& locations{attack_view.locations};
        auto const& target_locations{attack_view.target_locations};
        auto const& target_velocities{attack_view.target_velocities};
        ml::detail::solve_intercept_times_soa_loop::solve_intercept_times(
            attack_view.intercept_times.data(),
            locations,
            target_locations,
            target_velocities,
            config.laser.projectile_speed);

        auto reposition_countdowns{ml::TickCountdownView<std::int16_t>{
            attack_view.attack_reposition_countdowns, reposition_restart_ticks_}};
        for (std::int32_t index{}; index < n_attack; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            auto const location{locations[index]};
            auto const target_location{target_locations[index]};
            auto const intercept_location{
                target_location + target_velocities[index] * attack_view.intercept_times[element]};
            attack_view.desired_aiming_directions.set(
                index, ml::native_math::safe_normal(intercept_location - location, 1.e-8f));

            if (!reposition_countdowns.try_consume(element)) {
                continue;
            }
            auto const target_to_move_distance{
                HMM_LenV3(target_location - attack_view.desired_move_locations[index])};
            if (target_to_move_distance >= inner_attack_distance &&
                target_to_move_distance <= outer_attack_distance) {
                continue;
            }
            auto const target_direction{
                ml::native_math::safe_normal(target_location - location, 1.e-8f)};
            attack_view.target_directions.set(index, target_direction);
            attack_view.desired_move_locations.set(
                index, target_location - target_direction * desired_attack_distance);
        }
    }

    auto const& locations{data.locations};
    auto const& destinations{data.desired_move_locations};
    ml::native_math::direction_and_distance(data.movement_directions.xs,
                                            data.movement_directions.ys,
                                            data.movement_directions.zs,
                                            data.move_distances.data(),
                                            locations.xs,
                                            locations.ys,
                                            locations.zs,
                                            destinations.xs,
                                            destinations.ys,
                                            destinations.zs,
                                            data.num());
    update_navigation_steering();
    if (do_move) {
        auto const& movement_directions{move_view.movement_directions};
        lerp_in_place(move_view.planned_aim_directions, movement_directions, d_turn);
    }
    if (do_attack) {
        for (std::int32_t index{}; index < n_attack; ++index) {
            auto const choice{attack_view.avoidance_choice_indices[index]};
            auto const desired_direction{is_avoidance_direction_choice(choice)
                                             ? attack_view.movement_directions[index]
                                             : attack_view.desired_aiming_directions[index]};
            auto const current_direction{attack_view.aim_directions[index]};
            attack_view.planned_aim_directions.set(
                index, current_direction + (desired_direction - current_direction) * d_turn);
        }
    }

    for (auto const task : {Task::MoveToDestination, Task::Attack}) {
        auto const& view{get_task_view(task)};
        auto const count{view.num()};
        for (std::int32_t index{}; index < count; ++index) {
            view.move_distances[index] =
                std::min(view.move_distances[index], view.speeds[index] * dt);
        }
    }

    for (std::int32_t index{}; index < n_attack; ++index) {
        auto const next_location{attack_view.locations[index] +
                                 attack_view.movement_directions[index] *
                                     attack_view.move_distances[index]};
        auto const distance_sq{HMM_LenSqrV3(next_location - attack_view.target_locations[index])};
        attack_view.target_distance_sq[index] = distance_sq;
        attack_view.target_distances[index] = std::sqrt(distance_sq);
    }
}
void Sim::apply_movement() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::apply_movement");
    auto const data{entity_buffers.current().get_view().columns()};
    collision_dirty_entities_.clear();
    auto const count{data.num()};
    for (std::int32_t index{}; index < count; ++index) {
        auto const direction{data.aim_directions[index]};
        auto const planned_direction{data.planned_aim_directions[index]};
        if (direction.X == planned_direction.X && direction.Y == planned_direction.Y &&
            direction.Z == planned_direction.Z) {
            continue;
        }
        auto const before{direction_to_rotation(direction)};
        auto const after{direction_to_rotation(planned_direction)};
        if (before.pitch != after.pitch || before.yaw != after.yaw || before.roll != after.roll) {
            collision_dirty_entities_.push_back(data.entity_handles[index]);
        }
    }
    data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    copy_vectors(data.aim_directions, data.planned_aim_directions.get_const_view());
    move(movement_tick_period_, get_task_view(Task::MoveToDestination));
    move(movement_tick_period_, get_task_view(Task::Attack));
    lasers::FrameSpawnRequests requests{&frame_memory_resource};
    for (auto const index : pending_fire_indices_) {
        auto const direction{data.aim_directions[index]};
        requests.add(data.locations[index] + direction * fire_point_distance_,
                     direction_to_rotation(direction),
                     data.velocities[index],
                     config.laser.damage,
                     config.laser.projectile_speed,
                     config.laser.max_distance,
                     data.entity_handles[index],
                     {data.teams[index], EntityType::Fighter});
    }
    laser_simulation.queue_laser_spawns(requests.get_const_view());
    pending_fire_indices_.clear();
}
void Sim::generate_fire_commands() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::generate_fire_commands");
    handle_firing(get_task_view(Task::Attack));
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::resolve_damage_events");

    auto const data{entity_buffers.current().get_view().columns()};
    batch::resolve_damage_events(entity_registry,
                                 agents_.indexes(),
                                 data.entity_handles,
                                 data.healths,
                                 local_indices_to_remove,
                                 entity_death_info);

    auto const& direct_damage{entity_registry.get_direct_damage_queue_view()};
    auto const damage_events{direct_damage.get_const_view()};
    auto const damage_count{damage_events.num()};
    for (std::int32_t event_index{}; event_index < damage_count; ++event_index) {
        auto const event_element{static_cast<std::size_t>(event_index)};
        auto const damaged_handle{damage_events.damaged_entities[event_element]};
        auto const fighter_index{find_index(damaged_handle)};
        if (fighter_index < 0 || is_dead(data.healths[fighter_index])) {
            continue;
        }

        auto const instigator{damage_events.instigators[event_element]};
        auto const source{agents_.read_alive(entity_registry.get_current_id(instigator))};
        if (!source) {
            continue;
        }
        if (source->team != data.teams[fighter_index]) {
            data.target_handles[fighter_index] = instigator;
        }
    }

    validate_array_sizes();
}
void Sim::update_entity_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::update_entity_registry");
    prepare_entity_update_data();
    EntityRegistry::ConstView const view{entity_buffers.current().get_const_view().entity_handles(),
                                         registry_update_data.get_const_view().columns()};
    entity_registry.queue_entity_updates(view, entity_death_info);
}
void Sim::cleanup_entities() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::cleanup_entities");

    remove_dead_entities();
    if (!local_indices_to_remove.empty()) {
        refresh_layout();
    }
    local_indices_to_remove.clear();
    entity_death_info.reset();
    validate_array_sizes();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::finish_action");
    profiling::plot("Sandbox/FighterCount", get_num_instances());
    validate_array_sizes();
}

/* **************************************** */
// Movement
/* **************************************** */
void Sim::move(float const dt, TaskView const& fighters) {
    assert(dt > 0.f);
    auto const count{fighters.num()};
    auto const directions{fighters.movement_directions.get_const_view()};
    FrameVectors3f previous_locations{&frame_memory_resource};
    previous_locations.set_num(count);
    copy_vectors(previous_locations.get_view(), fighters.locations.get_const_view());
    for (std::int32_t index{}; index < count; ++index) {
        auto const move_distance{fighters.move_distances[index]};
        auto const velocity_scale{move_distance / dt};
        fighters.velocities.xs[index] = directions.xs[index] * velocity_scale;
        fighters.velocities.ys[index] = directions.ys[index] * velocity_scale;
        fighters.velocities.zs[index] = directions.zs[index] * velocity_scale;
    }
    ml::native_math::add_scaled_product_in_place(fighters.locations.xs,
                                                 fighters.locations.ys,
                                                 fighters.locations.zs,
                                                 directions.xs,
                                                 directions.ys,
                                                 directions.zs,
                                                 fighters.move_distances.data(),
                                                 1.f,
                                                 count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const before{previous_locations.get_const_view()[index]};
        auto const after{fighters.locations[index]};
        if (before.X != after.X || before.Y != after.Y || before.Z != after.Z) {
            collision_dirty_entities_.push_back(fighters.entity_handles[index]);
        }
    }
}
void Sim::update_navigation_steering() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::update_navigation_steering");

    auto const clearance{collision_radius_ + config.avoidance_clearance_buffer};
    auto const minimum_lookahead_distance{collision_radius_ * 2.f};
    auto const avoidance_lookahead_time{
        std::max(config.avoidance_lookahead_time, minimum_navigation_lookahead_time)};
    auto const active_update_interval{
        static_cast<float>(get_navigation_tick_period(NavigationRiskTier::Active) *
                           simulation_clock.get_tick_period())};
    auto const safe_progress_time{active_update_interval * 1.25f};
    navigation_telemetry = {};
    NavigationScratch scratch{&frame_memory_resource};

    // Only expired countdowns observe the world. Held steering is applied to every mover.
    collect_navigation_updates(scratch);
    update_separation_observations(scratch);
    apply_separation_steering();

    // Hard sweeps have final authority over the traffic-biased preferred direction.
    scan_preferred_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance);
    scan_alternative_navigation(
        scratch, clearance, avoidance_lookahead_time, minimum_lookahead_distance);
    select_navigation_alternatives(scratch, safe_progress_time);
    apply_navigation_choices(scratch);
    publish_navigation_telemetry();
}
void Sim::collect_navigation_updates(NavigationScratch& scratch) {
    auto const data{entity_buffers.current().get_view().columns()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    ml::PeriodicTickCountdownView<std::int16_t> const countdowns{
        data.navigation_update_countdowns_remaining_ticks,
        data.navigation_update_countdowns_periods};
    auto const count{static_cast<std::int32_t>(countdowns.num())};
    scratch.ready_fighter_indices.reserve(count);
    scratch.observed_risk_tiers.set_num(count);

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= count);
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            if (!countdowns.try_consume(element)) {
                continue;
            }

            scratch.ready_fighter_indices.add(index);
            scratch.observed_risk_tiers[index] =
                static_cast<std::uint8_t>(NavigationRiskTier::Clear);
        }
    }
}
void Sim::update_separation_observations(NavigationScratch& scratch) {
    auto const data{entity_buffers.current().get_view().columns()};
    // Fixed capacity bounds scoring work and keeps neighbour storage off the heap.
    std::array<RegistryEntityHandle, max_separation_neighbours> nearby_fighters;
    std::array<SeparationNeighbour, max_separation_neighbours> neighbours;
    auto const separation_radius{config.separation_radius};
    auto const immediate_distance{collision_radius_ * 2.f};
    auto const close_distance{std::max(separation_radius * 0.5f, immediate_distance)};
    auto const immediate_distance_sq{immediate_distance * immediate_distance};
    auto const close_distance_sq{close_distance * close_distance};

    for (auto const fighter_index : scratch.ready_fighter_indices) {
        auto const goal_direction{data.movement_directions[fighter_index]};
        auto const move_distance{data.move_distances[fighter_index]};
        if (ml::native_math::is_nearly_zero(
                goal_direction.X, goal_direction.Y, goal_direction.Z, 1.e-4f) ||
            move_distance <= 0.f) {
            data.separation_steering.set(fighter_index, Vector3f{});
            data.avoidance_choice_indices[fighter_index] = direct_movement_choice;
            data.avoidance_clear_scan_counts[fighter_index] = 0;
            continue;
        }

        auto const fighter_location{data.locations[fighter_index]};
        auto const fighter_handle{data.entity_handles[fighter_index]};
        auto const n_nearby{
            spatial_query_manager.collect_entities_of_type_in_range(fighter_location,
                                                                    EntityType::Fighter,
                                                                    separation_radius,
                                                                    fighter_handle,
                                                                    nearby_fighters)};
        ++navigation_telemetry.separation_query_count;
        navigation_telemetry.separation_candidate_count += n_nearby;

        std::int32_t neighbour_count{};
        for (std::int32_t index{}; index < n_nearby; ++index) {
            auto const local_index{find_index(nearby_fighters[index])};
            if (local_index >= 0 && is_alive(data.healths[local_index])) {
                neighbours[neighbour_count++] = {data.entity_ids[local_index],
                                                 data.locations[local_index]};
            }
        }

        auto const previous_memory{data.separation_steering[fighter_index]};
        auto const current_tier{
            static_cast<NavigationRiskTier>(data.navigation_risk_tiers[fighter_index])};
        auto const elapsed_since_scan{static_cast<float>(get_navigation_tick_period(current_tier)) *
                                      simulation_clock.get_tick_period()};
        auto const memory_retention{
            config.steering_memory_duration > 0.f
                ? static_cast<float>(std::clamp(
                      1.0 - elapsed_since_scan / config.steering_memory_duration, 0.0, 1.0))
                : 0.f};
        auto const observation{fighters::observe_separation(
            fighter_location,
            data.entity_ids[fighter_index],
            goal_direction,
            previous_memory,
            {neighbours.data(), static_cast<std::size_t>(neighbour_count)},
            {
                .separation_radius = separation_radius,
                .immediate_distance_squared = immediate_distance_sq,
                .close_distance_squared = close_distance_sq,
                .memory_retention = static_cast<float>(memory_retention),
                .separation_strength = config.separation_strength,
                .dense_traffic_neighbour_threshold = config.dense_traffic_neighbour_threshold,
                .integral_bias = data.integral_biases[fighter_index],
                .float_bias = data.float_biases[fighter_index],
            })};
        if (observation.dense_direction_selected) {
            ++navigation_telemetry.dense_direction_selection_count;
        }
        data.separation_steering.set(fighter_index, observation.steering_memory);
        scratch.observed_risk_tiers[fighter_index] =
            static_cast<std::uint8_t>(observation.risk_tier);
    }
}
void Sim::apply_separation_steering() {
    auto const data{entity_buffers.current().get_view().columns()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= data.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const direction{make_separation_steering_direction(data.movement_directions[index],
                                                                    data.separation_steering[index],
                                                                    config.separation_strength)};
            if (direction) {
                data.movement_directions.set(index, *direction);
            }
        }
    }
}
void Sim::scan_preferred_navigation(NavigationScratch& scratch,
                                    float const clearance,
                                    float const avoidance_lookahead_time,
                                    float const minimum_lookahead_distance) {
    auto const data{entity_buffers.current().get_view().columns()};
    auto const count{scratch.ready_fighter_indices.num()};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_fighter_indices.reserve(count);
    scratch.blocked_fighter_indices.reserve(count);

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const direction{data.movement_directions[index]};
        auto const move_distance{data.move_distances[element]};
        if (ml::native_math::is_nearly_zero(direction.X, direction.Y, direction.Z, 1.e-4f) ||
            move_distance <= 0.f) {
            continue;
        }

        auto const speed_distance{data.speeds[element] * avoidance_lookahead_time};
        auto const requested_distance{std::max(speed_distance, minimum_lookahead_distance)};
        auto const distance{std::min(move_distance, requested_distance)};
        auto const start{data.locations[index]};
        scratch.line_of_sight_starts.add(start);
        scratch.line_of_sight_ends.add(start + direction * distance);
        scratch.trace_fighter_indices.add(index);
    }

    auto const n_direct_traces{scratch.trace_fighter_indices.num()};
    if (n_direct_traces > 0) {
        execute_navigation_sweeps(scratch, clearance);

        for (std::int32_t index{}; index < n_direct_traces; ++index) {
            auto const fighter_index{scratch.trace_fighter_indices[index]};
            auto const element{static_cast<std::size_t>(fighter_index)};
            if (scratch.line_of_sight_results[index] == 0 || scratch.trace_hits.hits[index] != 0) {
                scratch.blocked_fighter_indices.add(fighter_index);
                data.avoidance_clear_scan_counts[element] = 0;
                continue;
            }
            if (data.avoidance_choice_indices[element] == direct_movement_choice) {
                data.avoidance_clear_scan_counts[element] = 0;
                continue;
            }

            auto& clear_count{data.avoidance_clear_scan_counts[element]};
            ++clear_count;
            if (clear_count >= clear_scans_to_end_avoidance) {
                data.avoidance_choice_indices[element] = direct_movement_choice;
                clear_count = 0;
            }
        }
    }
}
void Sim::scan_alternative_navigation(NavigationScratch& scratch,
                                      float const clearance,
                                      float const avoidance_lookahead_time,
                                      float const minimum_lookahead_distance) {
    auto const data{entity_buffers.current().get_const_view().columns()};
    scratch.trace_fighter_indices.clear();
    scratch.trace_choice_indices.clear();
    scratch.line_of_sight_starts.clear();
    scratch.line_of_sight_ends.clear();
    scratch.line_of_sight_results.clear();
    scratch.trace_hits.clear();

    auto const count{scratch.blocked_fighter_indices.num() * n_avoidance_choices};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_choice_indices.reserve(count);
    for (auto const index : scratch.blocked_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const speed_distance{data.speeds[element] * avoidance_lookahead_time};
        auto const requested_distance{std::max(speed_distance, minimum_lookahead_distance)};
        auto const lookahead_distance{std::min(data.move_distances[element], requested_distance)};
        auto const start{data.locations[index]};
        auto const frame{
            make_avoidance_frame(data.movement_directions[index], data.float_biases[element])};
        std::array<Vector3f, n_avoidance_choices> directions;
        make_avoidance_directions(frame, directions);
        std::array<std::int8_t, n_avoidance_choices> order;
        make_avoidance_choice_order(
            data.integral_biases[element], data.avoidance_choice_indices[element], order);

        for (auto const choice : order) {
            auto const direction{directions[static_cast<std::size_t>(choice)]};
            scratch.line_of_sight_starts.add(start);
            scratch.line_of_sight_ends.add(start + direction * lookahead_distance);
            scratch.trace_choice_indices.add(choice);
        }
    }
    assert(scratch.line_of_sight_ends.num() == count);
    execute_navigation_sweeps(scratch, clearance);
}
void Sim::execute_navigation_sweeps(NavigationScratch& scratch, float const clearance) {
    auto const trace_count{scratch.line_of_sight_ends.num()};
    if (trace_count == 0) {
        return;
    }
    auto const moving_half_extent{ml::make_vector3f(clearance, clearance, clearance)};
    scratch.line_of_sight_results.set_num(trace_count);
    spatial_query_manager.are_spheres_in_bounds(
        scratch.line_of_sight_ends.get_const_view(),
        clearance,
        {scratch.line_of_sight_results.data(),
         static_cast<std::size_t>(scratch.line_of_sight_results.num())});
    scratch.trace_hits.set_num(trace_count);
    // Fighters contribute soft steering; solid entities (including the parent capital) block.
    spatial_query_manager.sweep_closest_aabbs(scratch.line_of_sight_starts.get_const_view(),
                                              scratch.line_of_sight_ends.get_const_view(),
                                              moving_half_extent,
                                              scratch.trace_hits.get_view(),
                                              {},
                                              collision::TraceEntityFilter::ExcludeFighters);
    navigation_telemetry.hard_trace_count += trace_count;
}
void Sim::select_navigation_alternatives(NavigationScratch& scratch,
                                         float const safe_progress_time) {
    auto const data{entity_buffers.current().get_view().columns()};
    if (!diagnostics_enabled_) {
        diagnostic_stop_reports = 0;
    }
    auto const n_blocked_fighters{scratch.blocked_fighter_indices.num()};
    for (std::int32_t blocked_index{}; blocked_index < n_blocked_fighters; ++blocked_index) {
        auto const fighter_index{scratch.blocked_fighter_indices[blocked_index]};
        auto const fighter_location{data.locations[fighter_index]};
        // Partial progress must leave room until the next active scan, including its margin.
        auto const safe_progress_distance{data.speeds[fighter_index] * safe_progress_time};

        auto const candidate_begin{blocked_index * n_avoidance_choices};
        auto const candidate_end{candidate_begin + n_avoidance_choices};
        auto const candidate_count{static_cast<std::size_t>(n_avoidance_choices)};
        auto const chosen_choice{fighters::choose_navigation_alternative(
            fighter_location,
            safe_progress_distance,
            {scratch.trace_choice_indices.data() + candidate_begin, candidate_count},
            {scratch.line_of_sight_results.data() + candidate_begin, candidate_count},
            scratch.trace_hits.hits.view().subspan(candidate_begin, candidate_count),
            scratch.trace_hits.locations.get_const_view().slice(candidate_begin,
                                                                n_avoidance_choices),
            stop_movement_choice)};

        data.avoidance_choice_indices[fighter_index] = chosen_choice;
        if (chosen_choice == stop_movement_choice &&
            diagnostics::take_report(diagnostics_enabled_, diagnostic_stop_reports, 8)) {
            ml::log_error(std::format(
                "[FighterStop] fighterRegistryIndex={} position={} destination={} "
                "preferred={} separation={} clearance={:.2f} safeTravel={:.2f} risk={}",
                data.entity_handles[fighter_index].index,
                diagnostic_detail::vector_string(fighter_location),
                diagnostic_detail::vector_string(data.desired_move_locations[fighter_index]),
                diagnostic_detail::vector_string(data.movement_directions[fighter_index]),
                diagnostic_detail::vector_string(data.separation_steering[fighter_index]),
                collision_radius_ + config.avoidance_clearance_buffer,
                safe_progress_distance,
                data.navigation_risk_tiers[fighter_index]));
            for (std::int32_t trace_index{candidate_begin}; trace_index < candidate_end;
                 ++trace_index) {
                ml::log_error(std::format(
                    "[FighterStop] choice={} end={} inWorld={} hit={} "
                    "blockerRegistryIndex={} staticIndex={} hitDistance={:.2f}",
                    scratch.trace_choice_indices[trace_index],
                    diagnostic_detail::vector_string(
                        scratch.line_of_sight_ends.get_const_view()[trace_index]),
                    scratch.line_of_sight_results[trace_index],
                    scratch.trace_hits.hits[trace_index],
                    scratch.trace_hits.entities[trace_index].index,
                    scratch.trace_hits.static_geometry_indices[trace_index],
                    scratch.trace_hits.hits[trace_index]
                        ? HMM_LenV3(fighter_location -
                                    scratch.trace_hits.locations.get_const_view()[trace_index])
                        : -1.f));
            }
        }
    }
}
void Sim::apply_navigation_choices(NavigationScratch const& scratch) {
    auto const data{entity_buffers.current().get_view().columns()};
    std::array const active_spans{get_task_span(Task::MoveToDestination),
                                  get_task_span(Task::Attack)};
    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto observed{static_cast<NavigationRiskTier>(scratch.observed_risk_tiers[index])};
        if (data.avoidance_choice_indices[element] != direct_movement_choice) {
            observed = std::max(observed, NavigationRiskTier::Active);
        }
        auto const update{update_navigation_risk(
            static_cast<NavigationRiskTier>(data.navigation_risk_tiers[element]),
            observed,
            data.navigation_lower_risk_scan_counts[element],
            lower_risk_scans_to_demote)};
        data.navigation_risk_tiers[element] = static_cast<std::uint8_t>(update.tier);
        data.navigation_lower_risk_scan_counts[element] = update.lower_risk_scan_count;
        auto const period{get_navigation_tick_period(update.tier)};
        data.navigation_update_countdowns_periods[element] = period;
        data.navigation_update_countdowns_remaining_ticks[element] = period;
    }

    for (auto const span : active_spans) {
        auto const end{span.end()};
        assert(span.offset >= 0 && span.count >= 0 && end <= data.num());
        for (auto index{span.offset}; index < end; ++index) {
            auto const element{static_cast<std::size_t>(index)};
            auto const steering{data.separation_steering[index]};
            if (!ml::native_math::is_nearly_zero(steering.X, steering.Y, steering.Z, 1.e-4f)) {
                ++navigation_telemetry.separating_fighter_count;
                ++navigation_telemetry.steering_memory_fighter_count;
            }
            auto const choice{data.avoidance_choice_indices[element]};
            if (is_avoidance_direction_choice(choice)) {
                auto const frame{make_avoidance_frame(data.movement_directions[index],
                                                      data.float_biases[element])};
                data.movement_directions.set(index, make_avoidance_direction(frame, choice));
                ++navigation_telemetry.avoiding_fighter_count;
            } else if (choice == stop_movement_choice) {
                data.movement_directions.set(index, HMM_V3(0.f, 0.f, 0.f));
                ++navigation_telemetry.avoiding_fighter_count;
            }

            switch (static_cast<NavigationRiskTier>(data.navigation_risk_tiers[element])) {
                case NavigationRiskTier::Clear:
                    ++navigation_telemetry.clear_risk_count;
                    break;
                case NavigationRiskTier::Nearby:
                    ++navigation_telemetry.nearby_risk_count;
                    break;
                case NavigationRiskTier::Active:
                    ++navigation_telemetry.active_risk_count;
                    break;
                case NavigationRiskTier::Immediate:
                    ++navigation_telemetry.immediate_risk_count;
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    }
}
void Sim::publish_navigation_telemetry() const {
    profiling::plot("Sandbox/FightersAvoiding", navigation_telemetry.avoiding_fighter_count);
    profiling::plot("Sandbox/FighterNavigationTraces", navigation_telemetry.hard_trace_count);
    profiling::plot("Sandbox/FightersSeparating", navigation_telemetry.separating_fighter_count);
    profiling::plot("Sandbox/FighterSeparationQueries",
                    navigation_telemetry.separation_query_count);
    profiling::plot("Sandbox/FighterSeparationCandidates",
                    navigation_telemetry.separation_candidate_count);
    profiling::plot("Sandbox/FighterDenseDirectionSelections",
                    navigation_telemetry.dense_direction_selection_count);
    profiling::plot("Sandbox/FighterSteeringMemory",
                    navigation_telemetry.steering_memory_fighter_count);
    profiling::plot("Sandbox/FighterNavigationClear", navigation_telemetry.clear_risk_count);
    profiling::plot("Sandbox/FighterNavigationNearby", navigation_telemetry.nearby_risk_count);
    profiling::plot("Sandbox/FighterNavigationActive", navigation_telemetry.active_risk_count);
    profiling::plot("Sandbox/FighterNavigationImmediate",
                    navigation_telemetry.immediate_risk_count);
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entity_buffers.current().num();
}
auto Sim::get_view(std::int32_t const offset, std::int32_t const width) -> EntityData::View {
    return entity_buffers.current().get_view(offset, width).columns();
}
auto Sim::get_const_view(std::int32_t const offset, std::int32_t const width) const
    -> EntityData::ConstView {
    return entity_buffers.current().get_const_view(offset, width).columns();
}
auto Sim::get_handles() const noexcept -> std::span<RegistryEntityHandle const> {
    return entity_buffers.current().get_const_view().entity_handles();
}
auto Sim::has_handle(RegistryEntityHandle const fighter_handle) const -> bool {
    return find_index(fighter_handle) != -1;
}
auto Sim::get_target_handles() const noexcept -> std::span<RegistryEntityHandle const> {
    return entity_buffers.current().get_const_view().target_handles();
}
auto Sim::get_target_handle(RegistryEntityHandle const fighter_handle) const noexcept
    -> RegistryEntityHandle {
    return entity_buffers.current().get_const_view().target_handles()[find_index(fighter_handle)];
}
auto Sim::get_target_location(RegistryEntityHandle const fighter_handle) const -> Vector3f {
    return entity_buffers.current().get_const_view().columns().target_locations[find_index(
        fighter_handle)];
}
auto Sim::get_tasks() const -> std::span<Task const> {
    return entity_buffers.current().get_const_view().tasks();
}
auto Sim::get_teams() const -> std::span<Team const> {
    return entity_buffers.current().get_const_view().teams();
}
auto Sim::get_task_spans() const -> TaskSpans {
    check_fighter_tasks();
    return task_spans;
}
auto Sim::get_task_counts() const -> TaskCounts {
    auto const data{entity_buffers.current().get_const_view().columns()};
    TaskCounts counts{};
    for (auto const task : data.tasks) {
        auto const group{static_cast<std::size_t>(task)};
        assert(group < n_task_types);
        ++counts[group];
    }
    return counts;
}
auto Sim::get_task_view(Task const task) noexcept -> TaskView {
    auto const span{get_task_span(task)};
    return entity_buffers.current().get_view(span.offset, span.count).columns();
}
auto Sim::get_const_task_view(Task const task) const noexcept -> ConstTaskView {
    auto const span{get_task_span(task)};
    return entity_buffers.current().get_const_view(span.offset, span.count).columns();
}
auto Sim::find_index(RegistryEntityHandle const fighter_handle) const noexcept -> std::int32_t {
    auto const id{entity_registry.get_current_id(fighter_handle)};
    return id.is_valid() && id.entity_type() == EntityType::Fighter ? agents_.indexes().find(id)
                                                                    : -1;
}
auto Sim::get_task_span(Task const task) const -> IndexSpan {
    return task_spans[std::to_underlying(task)];
}

/* **************************************** */
// Targets
/* **************************************** */
void Sim::set_target_handle_unchecked(std::int32_t const fighter_index,
                                      RegistryEntityHandle const new_target) noexcept {
    entity_buffers.current().get_view().target_handles()[fighter_index] = new_target;
}
void Sim::set_target_handle(RegistryEntityHandle const fighter_handle,
                            RegistryEntityHandle const new_target) noexcept {
    set_target_handle_unchecked(find_index(fighter_handle), new_target);
}
void Sim::refresh_target_data() {
    auto const data{entity_buffers.current().get_view().columns()};
    auto const count{data.num()};
    ml::FrameArray<EntityUniqueId> target_ids{&frame_memory_resource};
    ml::FrameArray<std::int32_t> order{&frame_memory_resource};
    ml::FrameArray<std::uint8_t> alive{&frame_memory_resource};
    target_ids.set_num(count);
    order.set_num(count);
    alive.set_num(count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const target_id{entity_registry.get_current_id(data.target_handles[index])};
        target_ids[index] = target_id;
        data.target_radii[index] =
            target_id.is_valid()
                ? spatial_query_manager.get_entity_type_radius(target_id.entity_type())
                : 0.f;
    }
    agents_.gather_targets(
        target_ids, order, {data.target_locations, data.target_velocities, {}, alive});
    for (std::int32_t index{}; index < count; ++index) {
        if (!alive[index]) {
            data.target_handles[index] = {};
            data.target_radii[index] = 0.f;
        }
    }
    distance_and_squared(data.target_distances,
                         data.target_distance_sq,
                         data.locations.get_const_view(),
                         data.target_locations.get_const_view());
}

/* **************************************** */
// Tasks
/* **************************************** */
void Sim::set_task_unchecked(std::int32_t const index, Task const task) noexcept {
    auto const data{entity_buffers.current().get_view().columns()};
    if (data.tasks[index] == task) {
        return;
    }

    data.tasks[index] = task;
    reset_navigation_state(
        index, task == Task::Standby ? NavigationRiskTier::Clear : NavigationRiskTier::Nearby);
}
void Sim::set_task(RegistryEntityHandle const handle, Task const task) noexcept {
    order_queue.add(
        entity_registry.get_current_id(handle), FighterOrder{.task = 1, .target = 0}, task, {});
}
/* **************************************** */
// Entity data
/* **************************************** */
void Sim::prepare_entity_update_data() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::prepare_entity_update_data");

    auto const data{entity_buffers.current().get_const_view().columns()};
    auto const n{get_num_instances()};
    registry_update_data.reset();
    if (n < 1) {
        return;
    }

    registry_update_data.add_uninitialised(n);
    auto const updates{registry_update_data.get_view().columns()};
    copy_vectors(updates.locations, data.locations);
    copy_vectors(updates.velocities, data.velocities);
    std::ranges::copy(data.healths, updates.healths.begin());
    std::ranges::copy(data.teams, updates.teams.begin());
    for (std::int32_t i{0}; i < n; ++i) {
        updates.rotations.set(i, direction_to_rotation(data.aim_directions[i]));
    }
    registry_update_data.get_const_view().columns().validate_array_sizes();
}
bool Sim::tasks_are_contiguous() const noexcept {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::tasks_are_contiguous");

    auto const data{entity_buffers.current().get_const_view().columns()};
    auto current_task{Task::Standby};
    for (auto const task : data.tasks) {
        if (task < current_task || task >= Task::COUNT) {
            return false;
        }
        current_task = task;
    }

    std::int32_t offset{};
    auto const counts{get_task_counts()};
    for (std::size_t group{}; group < n_task_types; ++group) {
        if (task_spans[group] != IndexSpan{offset, counts[group]}) {
            return false;
        }
        offset += counts[group];
    }
    return true;
}
void Sim::refresh_layout() {
    agents_.indexes().assert_structural_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::refresh_layout");

    auto const task_counts{get_task_counts()};
    auto const n_fighters{get_num_instances()};
    TaskCounts write_indices{};
    std::int32_t offset{};
    for (std::size_t group{}; group < n_task_types; ++group) {
        write_indices[group] = offset;
        task_spans[group] = {offset, task_counts[group]};
        offset += task_counts[group];
    }
    assert(task_spans.back().end() == n_fighters);

    entity_buffers.cycle();
    auto const& old_data{entity_buffers.previous()};
    auto& new_data{entity_buffers.current()};
    new_data.reset();
    new_data.reserve(n_fighters);
    auto const old_tasks{old_data.get_const_view().tasks()};
    for (std::size_t group{}; group < n_task_types; ++group) {
        for (std::int32_t index{}; index < n_fighters; ++index) {
            if (static_cast<std::size_t>(old_tasks[index]) == group) {
                new_data.append_from(old_data.slice(index, 1));
                ++write_indices[group];
            }
        }
    }
    assert(old_data.num() == new_data.num());
    check_fighter_tasks();
}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::queue_spawns(FighterSpawnQueueConstView const new_spawns) -> std::int32_t {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::queue_spawns");
    new_spawns.validate_array_sizes();
    if (new_spawns.teams.empty()) {
        return 0;
    }
    auto const team{new_spawns.teams.front()};
    auto const team_index{static_cast<std::size_t>(team)};
    if (team_index >= participant_mask.size() || participant_mask[team_index] == 0) {
        ml::log_error(
            std::format("Rejected fighter spawn request for invalid or non-participating team {}",
                        static_cast<std::int32_t>(new_spawns.teams[0])));
        return 0;
    }

    for (auto const queued_team : new_spawns.teams) {
        if (queued_team != team) {
            ml::log_error("Rejected fighter spawn wave containing multiple teams");
            return 0;
        }
    }

    auto& capacity{remaining_team_capacity[team_index]};
    assert(capacity >= 0);
    auto const accepted_count{std::min(new_spawns.num(), capacity)};
    capacity -= accepted_count;
    if (accepted_count > 0) {
        append_fighter_spawns(spawn_queue, new_spawns.left(accepted_count));
    }
    return accepted_count;
}
void Sim::commit_spawns() {
    agents_.indexes().assert_preparation_mutation_allowed();
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::commit_spawns");

    if (!diagnostics_enabled_) {
        diagnostic_spawn_reports = 0;
    }
    new_spawn_entity_handles.reset();
    new_spawn_entity_data.reset();
    auto accepted_count{0};
    auto const pending_count{spawn_queue.num()};
    auto pending{spawn_queue.get_view().columns()};
    for (auto index{0}; index < pending_count; ++index) {
        if (entity_registry.is_valid_alive(pending.parents[index])) {
            pending.set(accepted_count++,
                        pending.locations.xs[index],
                        pending.locations.ys[index],
                        pending.locations.zs[index],
                        pending.rotations.pitches[index],
                        pending.rotations.yaws[index],
                        pending.rotations.rolls[index],
                        pending.teams[index],
                        pending.parents[index],
                        pending.targets[index]);
        }
    }
    spawn_queue.set_num(accepted_count);
    auto const n_cur{get_num_instances()};
    auto const n_new{spawn_queue.num()};

    spawn_queue.get_const_view().columns().validate_array_sizes();
    if (n_new < 1) {
        return;
    }

    entity_buffers.current().add_defaulted(n_new);
    auto const data{entity_buffers.current().get_view().columns()};
    auto const new_data{data.get_view(n_cur, n_new)};
    auto const spawns{spawn_queue.get_const_view().columns()};
    auto const navigation_period{get_navigation_tick_period(NavigationRiskTier::Nearby)};
    for (std::int32_t index{}; index < n_new; ++index) {
        auto const location{spawns.locations[index]};
        new_data.tasks[index] = FighterTask::Attack;
        new_data.locations.set(index, location);
        new_data.desired_move_locations.set(index, location);
        new_data.aim_directions.set(index, forward_direction(spawns.rotations[index]));
        new_data.speeds[index] = config.speed;
        new_data.teams[index] = spawns.teams[index];
        new_data.healths[index] = config.health;
        new_data.parent_handles[index] = spawns.parents[index];
        new_data.target_handles[index] = spawns.targets[index];
        new_data.navigation_risk_tiers[index] =
            static_cast<std::uint8_t>(NavigationRiskTier::Nearby);
        new_data.avoidance_choice_indices[index] = direct_movement_choice;
        data.navigation_update_countdowns_periods[n_cur + index] = navigation_period;
    }

    new_spawn_entity_data.add_uninitialised(n_new);
    auto const new_spawn_columns{new_spawn_entity_data.get_view().columns()};
    for (std::int32_t i{0}; i < n_new; ++i) {
        auto const index{n_cur + i};
        new_spawn_columns.locations.set(i, data.locations[index]);
        new_spawn_columns.rotations.set(i, direction_to_rotation(data.aim_directions[index]));
        new_spawn_columns.healths[i] = data.healths[index];
        new_spawn_columns.teams[i] = data.teams[index];
    }
    std::ranges::fill(new_spawn_columns.entity_types, EntityType::Fighter);
    new_spawn_columns.velocities.each_column(
        [](auto const column) { std::ranges::fill(column, 0.f); });

    new_spawn_entity_handles =
        entity_registry.add_entities(new_spawn_entity_data.get_const_view().columns());
    auto const& registry_handles{new_spawn_entity_handles.registry_handles};
    for (std::int32_t i{}; i < n_new; ++i) {
        data.entity_ids[n_cur + i] = new_spawn_entity_handles.get_id(i);
        data.entity_handles[n_cur + i] = {registry_handles.registry_indices[i],
                                          registry_handles.generations[i]};
    }
    if (diagnostics_enabled_) {
        for (std::int32_t i{}; i < n_new; ++i) {
            if (!diagnostics::take_report(diagnostics_enabled_, diagnostic_spawn_reports, 64)) {
                break;
            }
            auto const index{n_cur + i};
            ml::log_error(std::format(
                "[FighterSpawn] Committed fighterRegistryIndex={} parentRegistryIndex={} "
                "targetRegistryIndex={} world={}",
                data.entity_handles[index].index,
                data.parent_handles[index].index,
                data.target_handles[index].index,
                diagnostic_detail::vector_string(data.locations[index])));
        }
    }
    make_deterministic_biases(
        std::span<std::int32_t const>{
            new_spawn_entity_handles.registry_handles.registry_indices.data(),
            static_cast<std::size_t>(n_new)},
        std::span<std::int32_t const>{new_spawn_entity_handles.registry_handles.generations.data(),
                                      static_cast<std::size_t>(n_new)},
        std::span<std::uint32_t>{data.integral_biases}.subspan(n_cur, n_new),
        std::span<float>{data.float_biases}.subspan(n_cur, n_new));

    validate_array_sizes();
}

/* **************************************** */
// Destruction
/* **************************************** */
void Sim::self_destruct_fighter(RegistryEntityHandle const handle) {
    auto const data{entity_buffers.current().get_view().columns()};
    auto const index{find_index(handle)};
    if (index < 0 || is_dead(data.healths[index])) {
        return;
    }
    data.healths[index] = 0;
    entity_death_info.add(DeathReason::Unknown, handle, {});
    if (!std::ranges::contains(local_indices_to_remove, index)) {
        local_indices_to_remove.push_back(index);
    }
}
void Sim::remove_dead_entities() {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::remove_dead_entities");
    auto& data{entity_buffers.current()};
    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    for (auto const index : local_indices_to_remove) {
        agents_.indexes().retire(data.get_const_view().entity_ids()[index]);
        data.remove_at_swap(index, 1);
    }
    validate_array_sizes();
}

/* **************************************** */
// Combat
/* **************************************** */
void Sim::handle_firing(TaskView const& data) {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::handle_firing");

    auto predicted_location = [&data](std::int32_t const index) {
        return data.locations[index] + data.movement_directions[index] * data.move_distances[index];
    };

    auto const n_ships{data.num()};
    auto const aim_threshold{config.fire_dot_product_threshold};
    auto const laser_max_distance{config.laser.max_distance};
    auto const laser_max_distance_sq{laser_max_distance * laser_max_distance};
    auto const desired_attack_distance{laser_max_distance *
                                       config.attack_distance_band.desired_ratio};
    auto const arrival_distance{config.arrival_distance};
    auto const attack_position_arrival_distance_sq{arrival_distance * arrival_distance};
    ml::FrameArray<float> aiming_dot_products{&frame_memory_resource};
    ml::FrameArray<std::int32_t> can_fire{&frame_memory_resource};
    FrameVectors3f line_of_sight_starts{&frame_memory_resource};
    FrameVectors3f line_of_sight_ends{&frame_memory_resource};
    ml::FrameArray<std::uint8_t> line_of_sight_results{&frame_memory_resource};
    ml::FrameArray<RegistryEntityHandle> firing_ignored_entities{&frame_memory_resource};
    ml::FrameArray<std::int32_t> firing_position_fighter_indices{&frame_memory_resource};
    FrameVectors3f firing_position_candidates{&frame_memory_resource};

    firing_position_fighter_indices.reserve(n_ships);
    firing_position_candidates.reserve(n_ships);

    auto const los_check_buffer{config.los_check_buffer};
    ml::TickCountdownView<std::int16_t> const cooldowns{
        std::span<std::int16_t>{data.attack_cooldowns}, attack_retry_cooldown_tick_value};
    aiming_dot_products.set_num(n_ships);
    ml::native_math::dot_product_vector(aiming_dot_products.data(),
                                        data.planned_aim_directions.xs,
                                        data.planned_aim_directions.ys,
                                        data.planned_aim_directions.zs,
                                        data.desired_aiming_directions.xs,
                                        data.desired_aiming_directions.ys,
                                        data.desired_aiming_directions.zs,
                                        n_ships);

    can_fire.reserve(n_ships);
    for (std::int32_t index{}; index < n_ships; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (!cooldowns.is_ready(element)) {
            continue;
        }
        if (data.target_distance_sq[element] > laser_max_distance_sq ||
            !data.target_handles[element].is_valid() ||
            aiming_dot_products[index] < aim_threshold) {
            cooldowns.restart_counter(element);
            continue;
        }

        can_fire.add(index);
    }

    auto const firing_count{can_fire.num()};
    line_of_sight_starts.set_num(firing_count);
    line_of_sight_ends.set_num(firing_count);
    firing_ignored_entities.set_num(firing_count);
    for (std::int32_t index{}; index < firing_count; ++index) {
        auto const fighter_index{can_fire[index]};
        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const direction{data.planned_aim_directions[fighter_index]};
        auto const end_offset{los_check_buffer + data.target_radii[element]};
        firing_ignored_entities[index] = data.entity_handles[element];
        line_of_sight_starts.set(
            index, predicted_location(fighter_index) + direction * fire_point_distance_);
        line_of_sight_ends.set(index,
                               data.target_locations[fighter_index] - direction * end_offset);
    }

    if (can_fire.is_empty()) {
        return;
    }

    auto const n_can_fire_before_los{can_fire.num()};
    line_of_sight_results.set_num(n_can_fire_before_los);

    spatial_query_manager.have_clear_lines(
        line_of_sight_starts.get_const_view(),
        line_of_sight_ends.get_const_view(),
        {line_of_sight_results.data(), static_cast<std::size_t>(line_of_sight_results.num())},
        {firing_ignored_entities.data(), static_cast<std::size_t>(firing_ignored_entities.num())});

    for (auto index{can_fire.num() - 1}; index >= 0; --index) {
        auto const fighter_index{can_fire[index]};
        if (line_of_sight_results[index] != 0) {
            continue;
        }

        can_fire.remove_at_swap(index);
        cooldowns.restart_counter(static_cast<std::size_t>(fighter_index));
        auto const offset{predicted_location(fighter_index) -
                          data.desired_move_locations[fighter_index]};
        if (HMM_LenSqrV3(offset) <= attack_position_arrival_distance_sq) {
            firing_position_fighter_indices.add(fighter_index);
        }
    }

    auto const n_fire_point_candidates{
        static_cast<std::uint32_t>(firing_detail::angle_offsets.size())};
    for (std::uint32_t candidate_order{};
         candidate_order < n_fire_point_candidates && !firing_position_fighter_indices.is_empty();
         ++candidate_order) {
        auto const n_fighters{firing_position_fighter_indices.num()};
        line_of_sight_starts.set_num(n_fighters);
        line_of_sight_ends.set_num(n_fighters);
        line_of_sight_results.set_num(n_fighters);
        firing_ignored_entities.set_num(n_fighters);
        firing_position_candidates.set_num(n_fighters);

        for (std::int32_t i{}; i < n_fighters; ++i) {
            auto const ship_index{firing_position_fighter_indices[i]};
            firing_ignored_entities[i] = data.entity_handles[ship_index];
            auto const candidate{firing_detail::make_fire_point_candidate(
                data.target_locations[ship_index],
                data.desired_move_locations[ship_index],
                fire_point_distance_,
                los_check_buffer + data.target_radii[ship_index],
                desired_attack_distance,
                data.integral_biases[ship_index],
                data.float_biases[ship_index],
                candidate_order)};
            line_of_sight_starts.set(i, candidate.trace_start);
            line_of_sight_ends.set(i, candidate.trace_end);
            firing_position_candidates.set(i, candidate.location);
        }

        spatial_query_manager.have_clear_lines(
            line_of_sight_starts.get_const_view(),
            line_of_sight_ends.get_const_view(),
            {line_of_sight_results.data(), static_cast<std::size_t>(line_of_sight_results.num())},
            {firing_ignored_entities.data(),
             static_cast<std::size_t>(firing_ignored_entities.num())});

        for (auto index{n_fighters - 1}; index >= 0; --index) {
            if (line_of_sight_results[index] == 0) {
                continue;
            }

            auto const fighter_index{firing_position_fighter_indices[index]};
            data.desired_move_locations.set(fighter_index,
                                            firing_position_candidates.get_const_view()[index]);
            firing_position_fighter_indices.remove_at_swap(index);
        }
    }

    auto const n_can_fire{can_fire.num()};
    auto const attack_offset{get_task_span(Task::Attack).offset};
    for (std::int32_t i{0}; i < n_can_fire; ++i) {
        auto const ship_index{can_fire[i]};
        pending_fire_indices_.push_back(attack_offset + ship_index);
        data.attack_cooldowns[ship_index] = attack_restart_ticks_;
    }
}

/* **************************************** */
// Orders
/* **************************************** */
void Sim::queue_orders(FighterOrderQueue const& queue) {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::queue_orders");
    order_queue.append_from(queue.get_const_view());
}
void Sim::commit_orders() {
    assert(simulation_clock.phase == SimulationPhase::Preparation);
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::commit_orders");

    auto const data{entity_buffers.current().get_view().columns()};
    auto const n_orders{order_queue.num()};
    if (n_orders < 1) {
        return;
    }

    auto const orders{order_queue.get_const_view()};
    for (std::int32_t index{}; index < n_orders; ++index) {
        auto const order_index{static_cast<std::size_t>(index)};
        auto const id{orders.entity_ids[order_index]};
        if (!id.is_valid() || id.entity_type() != EntityType::Fighter) {
            continue;
        }
        auto const fighter_index{agents_.indexes().find(id)};
        if (fighter_index < 0 || is_dead(data.healths[fighter_index])) {
            continue;
        }

        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const order{orders.orders[order_index]};
        if (order.task) {
            auto const old_task{data.tasks[element]};
            auto const new_task{orders.tasks[order_index]};
            data.tasks[element] = new_task;
            reset_navigation_state(fighter_index,
                                   new_task == FighterTask::Standby ? NavigationRiskTier::Clear
                                                                    : NavigationRiskTier::Nearby);
            if (old_task != FighterTask::Attack && new_task == FighterTask::Attack) {
                data.desired_move_locations.set(fighter_index, data.locations[fighter_index]);
                data.attack_reposition_countdowns[element] = 0;
            }
        }
        if (order.target) {
            data.target_handles[element] = orders.targets[order_index];
        }
    }
    order_queue.reset();
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    local_indices_to_remove.clear();
    entity_death_info.reset();
    spawn_queue.reset();
}

/* **************************************** */
// Checks
/* **************************************** */
#ifndef NDEBUG
void Sim::validate_array_sizes() const {
    entity_buffers.current().get_const_view().columns().validate_array_sizes();
}
void Sim::check_fighter_tasks() const {
    SANDBOX_PROFILE_SCOPE("Sandbox::fighters::Sim::check_fighter_tasks");

    auto current_task_group{Task::Standby};
    TaskSpans checked_task_spans{};
    auto const data{entity_buffers.current().get_const_view().columns()};
    auto const n_tasks{data.num()};
    for (std::int32_t i{}; i < n_tasks; ++i) {
        auto const task{data.tasks[i]};
        auto const task_value{std::to_underlying(task)};
        if (task == current_task_group) {
            ++checked_task_spans[task_value].count;
        } else if (task > current_task_group) {
            current_task_group = task;
            checked_task_spans[task_value].offset = i;
            checked_task_spans[task_value].count = 1;
        } else {
            ml::fatal_error(std::format("Found task {} when current group was {}",
                                        to_string_view(task),
                                        to_string_view(current_task_group)));
        }
    }

    for (std::size_t i{1}; i < n_task_types; ++i) {
        auto const last_span{checked_task_spans[i - 1]};
        auto const last_end{last_span.end()};
        auto& current_span{checked_task_spans[i]};
        if (current_span.offset < last_end) {
            current_span.offset = last_end;
            assert(current_span.count == 0);
        }
    }

    if (checked_task_spans != task_spans) {
        std::string message{"Incorrect task spans."};
        for (std::size_t i{}; i < n_task_types; ++i) {
            message += std::format("\\n    {}: expected ({}, {}), got ({}, {})",
                                   to_string_view(static_cast<Task>(i)),
                                   task_spans[i].offset,
                                   task_spans[i].count,
                                   checked_task_spans[i].offset,
                                   checked_task_spans[i].count);
        }
        ml::fatal_error(message);
    }
}
#endif
} // namespace fighters

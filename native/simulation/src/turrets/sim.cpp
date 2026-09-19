#include "ioj/sim/turrets/sim.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <execution>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/deterministic_bias.h>
#include <numeric>
#include <sandbox/core/countdown.h>
#include <sandbox/core/fixed_array.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/projectile_intercept.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/core/vector_math.h>
#include <sandbox/core/vector_normalization.h>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_config.h>
#include <ioj/sim/spatial_query_manager.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/core/loop_bounds.h>

namespace ioj::sim::turrets {
/* **************************************** */
// Configuration
/* **************************************** */
void Sim::set_config(TurretSimConfig const& new_config) noexcept {
    config = new_config;
}
Sim::Sim(SimClock const& clock,
         EntityLedger& ledger,
         CombatEvents const& combat_events,
         EntityTables& entity_tables,
         AgentAccessor const& agents,
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_laser_simulation) noexcept
    : simulation_clock{clock}
    , ledger_{ledger}
    , combat_events_{combat_events}
    , entity_tables_{entity_tables}
    , agents_{agents}
    , spatial_query_manager{in_spatial_query_manager}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::register_turrets(TurretSpawnDataConstView const spawn_data,
                           Rotators3fConstView const rotations) -> std::vector<EntityUniqueId> {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::register_turrets");
    agents_.indexes().assert_preparation_mutation_allowed();
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    assert(config.target_refresh_frequency > 0.f);
    auto const target_refresh_tick_period_unsigned{
        simulation_clock.frequency_to_tick_period(config.target_refresh_frequency)};
    assert(target_refresh_tick_period_unsigned > 0 &&
           std::in_range<std::int16_t>(target_refresh_tick_period_unsigned));
    auto const target_refresh_tick_period{
        static_cast<std::int16_t>(target_refresh_tick_period_unsigned)};

    auto const first_new_index{entities.num()};
    entities.add_defaulted(n_to_add);
    auto const entities{this->entities.get_view().columns()};
    auto const spawn_count{static_cast<std::size_t>(n_to_add)};
    for (std::int32_t local_index{}; local_index < n_to_add; ++local_index) {
        auto const index{first_new_index + local_index};
        auto const location{spawn_data.locations[local_index]};
        entities.locations.set(index, location);
        entities.fire_point_locations.set(index, location + config.fire_point_offset);
        entities.teams[index] = spawn_data.teams[local_index];
        entities.laser_damages[index] = spawn_data.laser_damages[local_index];
        entities.target_refresh_countdowns_periods[index] = target_refresh_tick_period;
        entities.target_refresh_countdowns_remaining_ticks[index] =
            static_cast<std::int16_t>(target_refresh_next_offset);
        ++target_refresh_next_offset;
        if (target_refresh_next_offset == target_refresh_tick_period) {
            target_refresh_next_offset = 0;
        }
    }

    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const rotation{rotations[i]};
        entities.rotations.set(first_new_index + i, rotation);
    }
    std::vector<EntityUniqueId> new_ids;
    new_ids.reserve(spawn_count);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const id{ledger_.record_spawn(
            EntityType::Turret, spawn_data.teams[i], is_alive(spawn_data.healths[i]))};
        new_ids.push_back(id);
        entities.entity_ids[first_new_index + i] = id;
    }
    entity_tables_.health.add(std::span<EntityUniqueId const>{entities.entity_ids}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count),
                              spawn_data.healths,
                              std::span<HealthIndex>{entities.health_indices}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count));
    make_deterministic_biases(std::span<EntityUniqueId const>{entities.entity_ids}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count),
                              std::span<std::uint32_t>{entities.integral_biases}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count));
    validate_array_sizes();
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Spawn,
                                  .index = index,
                                  .location = entities.locations[index],
                                  .rotation = entities.rotations[index],
                                  .team = entities.teams[index],
                                  .id = entities.entity_ids[index]});
    }
    return new_ids;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Sim::handle_dead_entities() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::handle_dead_entities");
    if (local_indices_to_remove.empty()) {
        return;
    }

    auto const entities{this->entities.get_const_view().columns()};
    for (auto const index : local_indices_to_remove) {
        frame_changes_.push_back({.kind = EntityFrameChangeKind::RemoveSwap,
                                  .index = index,
                                  .id = entities.entity_ids[index]});
    }
    for (auto const index : local_indices_to_remove) {
        agents_.indexes().retire(entities.entity_ids[index]);
    }
    this->entities.remove_at_swap(local_indices_to_remove);
}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::begin_play");
    profiling::plot("Sandbox/TurretCount", 0);
    assert(config.search_slice_size > 0);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
    validate_array_sizes();
}
void Sim::prepare_tick(float const) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::prepare_tick");
    clear_tick_buffers();

    auto const entities{this->entities.get_view().columns()};
    ml::tick_countdowns<std::int16_t>(entities.laser_cooldowns, cooldown_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(entities.target_refresh_countdowns_remaining_ticks);
}
void Sim::refresh_target_data(ml::FrameScratch& scratch) {
    auto const entities{this->entities.get_view().columns()};
    auto const count{entities.num()};
    ml::FrameArray<std::int32_t> order{&scratch};
    ml::FrameArray<std::uint8_t> alive{&scratch};
    order.set_num(count);
    alive.set_num(count);
    agents_.gather_targets(entities.target_ids,
                           order,
                           {entities.target_locations, entities.target_velocities, {}, alive});
    for (std::int32_t index{}; index < count; ++index) {
        if (!alive[index]) {
            entities.target_ids[index] = {};
        }
    }
}
void Sim::think(float const, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::think");
    refresh_target_data(scratch);
    perform_search(scratch);
    refresh_target_data(scratch);
}
void Sim::generate_fire_commands(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::generate_fire_commands");

    fire_at_enemies(scratch);
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::resolve_damage_events");

    auto const entities{this->entities.get_view().columns()};
    auto const healths{
        entity_tables_.health.get_view(entities.health_indices, entities.entity_ids)};
    batch::resolve_damage_events(combat_events_.events_for(EntityType::Turret),
                                 agents_.indexes(),
                                 entities.entity_ids,
                                 healths,
                                 local_indices_to_remove,
                                 entity_death_info,
                                 ledger_);
    for (auto const index : local_indices_to_remove) {
        death_locations_.push_back(entities.locations[index]);
    }
    validate_array_sizes();
}
void Sim::publish_deaths() {
    auto const deaths{entity_death_info.get_const_view()};
    for (std::int32_t i{}; i < deaths.num(); ++i) {
        ledger_.record_death(deaths.victims[i], deaths.killers[i], deaths.reasons[i]);
    }
}
void Sim::remove_components() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("turrets::Sim::remove_components");

    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    if (local_indices_to_remove.empty()) {
        return;
    }

    auto const entities{this->entities.get_const_view().columns()};
    entity_tables_.remove_health_rows(
        local_indices_to_remove, entities.health_indices, entities.entity_ids);
}
void Sim::remove_entities() {
    agents_.indexes().assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("turrets::Sim::remove_entities");

    handle_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::finish_action");
    profiling::plot("Sandbox/TurretCount", get_num_instances());

    validate_array_sizes();
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}
auto Sim::get_target_ids() const -> std::span<EntityUniqueId const> {
    return entities.get_const_view().target_ids();
}

/* **************************************** */
// Searching
/* **************************************** */
void Sim::perform_search(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::perform_search");

    auto const n_turrets{get_num_instances()};
    if (n_turrets == 0) {
        return;
    }

    auto const radius{config.detection_radius};

    auto const hardware_thread_count{
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency()))};
    auto const max_jobs_for_grain_size{std::max(1, n_turrets / config.search_slice_size)};
    auto const n_jobs{std::min(hardware_thread_count, max_jobs_for_grain_size)};
    auto const turrets_per_job{(n_turrets + n_jobs - 1) / n_jobs};

    ml::FrameArray<std::int32_t> jobs{&scratch};
    jobs.set_num(n_jobs);
    auto const job_indices{jobs.view()};
    std::iota(job_indices.begin(), job_indices.end(), 0);
    std::for_each(std::execution::par,
                  job_indices.begin(),
                  job_indices.end(),
                  [=, this](std::int32_t const i) {
                      perform_search_on_slice(i, n_turrets, turrets_per_job, radius);
                  });
}
void Sim::perform_search_on_slice(std::int32_t const job_index,
                                  std::int32_t const n_turrets,
                                  std::int32_t const turrets_per_job,
                                  float const radius) {
    auto const begin{job_index * turrets_per_job};
    auto const end{std::min(begin + turrets_per_job, n_turrets)};
    std::array<float, 128> candidate_xs;
    std::array<float, 128> candidate_ys;
    std::array<float, 128> candidate_zs;
    ml::FixedArray<std::uint8_t, 128> has_line_of_sight;
    auto const entities{this->entities.get_view().columns()};

    ml::PeriodicTickCountdownView<std::int16_t> const refresh_countdowns{
        entities.target_refresh_countdowns_remaining_ticks,
        entities.target_refresh_countdowns_periods};
    for (std::int32_t i{begin}; i < end; ++i) {
        if (!refresh_countdowns.try_consume(i)) {
            continue;
        }

        if (!entities.target_ids[i].is_valid()) {
            auto const turret_location{entities.locations[i]};
            auto const this_team{entities.teams[i]};

            ml::FixedArray<EntityUniqueId, 128> target_ids;
            target_ids.set_num_uninitialised(
                spatial_query_manager.collect_non_team_entities_in_range(
                    turret_location, this_team, radius, target_ids.capacity_view()));

            entities.target_ids[i] = EntityUniqueId{};

            auto const target_count{target_ids.num()};
            auto const count{static_cast<std::size_t>(target_count)};
            has_line_of_sight.set_num_uninitialised(target_count);
            Vectors3fView const candidate_locations_view{std::span{candidate_xs}.first(count),
                                                         std::span{candidate_ys}.first(count),
                                                         std::span{candidate_zs}.first(count)};
            std::array<std::int32_t, 128> order{};
            std::array<Team, 128> teams{};
            std::array<std::uint8_t, 128> alive{};
            agents_.gather_targets(target_ids,
                                   std::span{order}.first(count),
                                   {candidate_locations_view,
                                    {},
                                    std::span{teams}.first(count),
                                    std::span{alive}.first(count)});

            spatial_query_manager.has_line_of_sight_to_targets(
                entities.fire_point_locations[i],
                candidate_locations_view.get_const_view(),
                target_ids,
                has_line_of_sight);

            if (target_count > 0) {
                auto const target_offset{static_cast<std::int32_t>(
                    entities.integral_biases[i] % static_cast<std::uint32_t>(target_count))};
                auto const loop_bounds{
                    ml::make_rotated_loop_bounds(0, target_count, target_offset)};
                for (auto const bounds : loop_bounds) {
                    for (auto candidate_index{bounds.begin}; candidate_index < bounds.end;
                         ++candidate_index) {
                        auto const element{static_cast<std::size_t>(candidate_index)};
                        if (has_line_of_sight[element] == 0) {
                            continue;
                        }

                        auto const candidate{target_ids[element]};
                        if (alive[element] && teams[element] != this_team) {
                            entities.target_ids[i] = candidate;
                            break;
                        }
                    }
                    if (entities.target_ids[i].is_valid()) {
                        break;
                    }
                }
            }
        }
    }
}

/* **************************************** */
// Attacking
/* **************************************** */
void Sim::fire_at_enemies(ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::fire_at_enemies");

    auto const count{get_num_instances()};
    ml::FrameArray<std::int32_t> candidate_indices{&scratch};
    ml::FrameArray<EntityUniqueId> hit_ids{&scratch};
    FrameVectors3f starts{scratch};
    FrameVectors3f ends{scratch};
    candidate_indices.reserve(count);
    starts.reserve(count);
    ends.reserve(count);

    auto const entities{this->entities.get_view().columns()};
    auto const disengage_radius{get_disengage_radius()};
    auto const disengage_radius_squared{disengage_radius * disengage_radius};
    auto cooldowns{
        ml::TickCountdownView<std::int16_t>{entities.laser_cooldowns, cooldown_restart_ticks_}};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto& target{entities.target_ids[element]};
        if (!target.is_valid()) {
            continue;
        }
        if (!agents_.read_alive(target)) {
            target = {};
            continue;
        }
        if (!cooldowns.is_ready(element)) {
            continue;
        }
        if (HMM_LenSqrV3(entities.locations[index] - entities.target_locations[index]) >=
            disengage_radius_squared) {
            target = {};
            continue;
        }

        candidate_indices.add(index);
        starts.add(entities.fire_point_locations[index]);
        ends.add(entities.target_locations[index]);
        cooldowns.restart_counter(element);
    }

    auto const candidate_count{candidate_indices.num()};
    if (candidate_count == 0) {
        return;
    }
    hit_ids.set_num(candidate_count);

    spatial_query_manager.trace_line_of_sight(
        starts.get_const_view(),
        ends.get_const_view(),
        {hit_ids.data(), static_cast<std::size_t>(candidate_count)});

    lasers::FrameSpawnRequests new_lasers{scratch};
    new_lasers.reserve(candidate_count);
    for (std::int32_t candidate{}; candidate < candidate_count; ++candidate) {
        auto const index{candidate_indices[candidate]};
        auto const element{static_cast<std::size_t>(index)};
        if (hit_ids[candidate] != entities.target_ids[element]) {
            continue;
        }

        auto const location{entities.fire_point_locations[index]};
        auto const target_location{entities.target_locations[index]};
        auto const target_velocity{entities.target_velocities[index]};
        auto const intercept_time{ml::solve_intercept_time(
            location, target_location, target_velocity, config.laser.projectile_speed)};
        auto const direction{ml::native_math::safe_normal(
            target_location + target_velocity * intercept_time - location, 1.e-8f)};
        Rotator3f rotation{};
        ml::native_math::to_rotations(&rotation.pitch,
                                      &rotation.yaw,
                                      &rotation.roll,
                                      &direction.X,
                                      &direction.Y,
                                      &direction.Z,
                                      1);
        new_lasers.add(location,
                       rotation,
                       HMM_V3(0.f, 0.f, 0.f),
                       entities.laser_damages[element],
                       config.laser.projectile_speed,
                       config.laser.max_distance,
                       entities.entity_ids[element],
                       {entities.teams[element], EntityType::Turret});
    }
    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}
auto Sim::get_disengage_radius() const -> float {
    return config.detection_radius * 1.2f;
}

/* **************************************** */
// Misc
/* **************************************** */
void Sim::clear_tick_buffers() {
    entity_death_info.reset();
    local_indices_to_remove.clear();
}

/* **************************************** */
// Checks
/* **************************************** */
void Sim::validate_array_sizes() const {
    entities.get_const_view().columns().validate_array_sizes();
}
} // namespace turrets

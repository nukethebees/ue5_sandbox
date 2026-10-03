#include "ioj/sim/turrets/sim.h"

#include <ioj/sim/batch_operations.h>
#include <ioj/sim/column_math.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/deterministic_bias.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/lasers/frame_scratch.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_config.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/countdown.h>
#include <sandbox/core/fixed_array.h>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/frame_memory_resource.h>
#include <sandbox/core/loop_bounds.h>
#include <sandbox/core/parallel_for.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/projectile_intercept.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/core/vector_math.h>
#include <sandbox/core/vector_normalization.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

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
         SpatialQueryManager const& in_spatial_query_manager,
         lasers::Sim& in_laser_simulation) noexcept
    : simulation_clock{clock}
    , ledger_{ledger}
    , combat_events_{combat_events}
    , entity_tables_{entity_tables}
    , spatial_query_manager{in_spatial_query_manager}
    , laser_simulation{in_laser_simulation} {}

/* **************************************** */
// Spawning
/* **************************************** */
auto Sim::register_turrets(LevelTurretSpawnEvents::ConstView const spawn_data)
    -> std::vector<EntityUniqueId> {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::register_turrets");
    entity_tables_.lookups.assert_preparation_mutation_allowed();
    spawn_data.validate();
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
    auto const entities{this->entities.get_view()};
    auto const spawn_count{static_cast<std::size_t>(n_to_add)};
    auto const entities_locations{entities.view_locations()};
    auto const entities_fire_point_locations{entities.view_fire_point_locations()};
    auto const entities_teams{entities.teams()};
    auto const entities_laser_damages{entities.laser_damages()};
    auto const entities_target_refresh_countdowns_periods{
        entities.target_refresh_countdowns_periods()};
    auto const entities_target_refresh_countdowns_remaining_ticks{
        entities.target_refresh_countdowns_remaining_ticks()};
    auto const entities_rotations{entities.view_rotations()};
    auto const entities_entity_ids{entities.entity_ids()};

    auto const spawn_data_locations{spawn_data.view_locations()};
    auto const spawn_data_teams{spawn_data.teams()};
    auto const spawn_data_laser_damages{spawn_data.laser_damages()};
    auto const spawn_data_rotations{spawn_data.view_rotations()};
    auto const spawn_data_healths{spawn_data.healths()};

    for (std::uint32_t local_index{}; local_index < n_to_add; ++local_index) {
        auto const index{first_new_index + local_index};
        auto const location{vector_at(spawn_data_locations, local_index)};
        set_vector(entities_locations, index, location);
        set_vector(entities_fire_point_locations, index, location + config.fire_point_offset);
        entities_teams[index] = spawn_data_teams[local_index];
        entities_laser_damages[index] = spawn_data_laser_damages[local_index];
        entities_target_refresh_countdowns_periods[index] = target_refresh_tick_period;
        entities_target_refresh_countdowns_remaining_ticks[index] =
            static_cast<std::int16_t>(target_refresh_next_offset);
        ++target_refresh_next_offset;
        if (target_refresh_next_offset == target_refresh_tick_period) {
            target_refresh_next_offset = 0;
        }
    }

    for (std::uint32_t i{}; i < n_to_add; ++i) {
        auto const rotation{rotation_at(spawn_data_rotations, i)};
        set_rotation(entities_rotations, first_new_index + i, rotation);
    }
    std::vector<EntityUniqueId> new_ids;
    new_ids.reserve(spawn_count);
    for (std::uint32_t i{}; i < n_to_add; ++i) {
        auto const id{ledger_.record_spawn(
            EntityType::Turret, spawn_data_teams[i], is_alive(spawn_data_healths[i]))};
        new_ids.push_back(id);
        entities_entity_ids[first_new_index + i] = id;
    }
    entity_tables_.health.initialise_rows<EntityType::Turret>(first_new_index, spawn_data_healths);
    make_deterministic_biases(std::span<EntityUniqueId const>{entities_entity_ids}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count),
                              std::span<std::uint32_t>{entities.integral_biases()}.subspan(
                                  static_cast<std::size_t>(first_new_index), spawn_count));
    for (std::uint32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EntityFrameChangeKind::Spawn,
                                  .index = index,
                                  .location = vector_at(entities_locations, index),
                                  .rotation = rotation_at(entities_rotations, index),
                                  .team = entities_teams[index],
                                  .id = entities_entity_ids[index]});
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

    auto const entities{this->entities.get_const_view()};
    auto const entity_ids{entities.entity_ids()};

    for (auto const index : local_indices_to_remove) {
        frame_changes_.push_back(
            {.kind = EntityFrameChangeKind::RemoveSwap, .index = index, .id = entity_ids[index]});
    }
    entity_tables_.lookups.for_type(EntityType::Turret)
        .retire_rows(entity_ids, local_indices_to_remove);
    this->entities.remove_at_swap(local_indices_to_remove);
}

/* **************************************** */
// Sim phases
/* **************************************** */
void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::begin_play");
    profiling::plot("Sandbox/TurretCount", 0);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
}
void Sim::update_entity_lookup_table() {
    entity_tables_.sources.turrets = &entities;
    auto const rows{entities.get_const_view()};
    entity_tables_.publish<EntityType::Turret>(rows.entity_ids(), rows.teams(), config.max_health);
}
void Sim::prepare_tick(float const) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::prepare_tick");
    clear_tick_buffers();

    auto const entities{this->entities.get_view()};
    ml::tick_countdowns<std::int16_t>(entities.laser_cooldowns(), cooldown_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(
        entities.target_refresh_countdowns_remaining_ticks());
    update_entity_lookup_table();
}
void Sim::refresh_target_data(ml::FrameScratchResource& scratch_resource) {
    auto const entities{this->entities.get_view()};
    auto const count{entities.num()};
    ml::FrameArray<std::uint32_t> order{&scratch_resource};
    ml::FrameArray<std::uint8_t> alive{&scratch_resource};
    order.set_num(count);
    alive.set_num(count);
    auto const target_ids{entities.target_ids()};
    auto const target_locations{entities.view_target_locations()};
    auto const target_velocities{entities.view_target_velocities()};

    gather_entities(entity_tables_,
                    target_ids,
                    order,
                    {{target_locations.xs(), target_locations.ys(), target_locations.zs()},
                     {target_velocities.xs(), target_velocities.ys(), target_velocities.zs()},
                     {},
                     alive});
    for (std::uint32_t index{}; index < count; ++index) {
        if (!alive[index]) {
            target_ids[index] = {};
        }
    }
}
void Sim::think(float const, ml::FrameScratchResource& scratch_resource) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::think");
    refresh_target_data(scratch_resource);
    perform_search();
    refresh_target_data(scratch_resource);
}
void Sim::generate_fire_commands(ml::FrameScratchResource& scratch_resource) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::generate_fire_commands");

    fire_at_enemies(scratch_resource);
}
void Sim::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::resolve_damage_events");

    auto const entities{this->entities.get_view()};
    auto const healths{entity_tables_.health.get_view<EntityType::Turret>(entities.num())};
    batch::resolve_damage_events(combat_events_.events_for(EntityType::Turret),
                                 entity_tables_.lookups.for_type(EntityType::Turret),
                                 entities.entity_ids(),
                                 healths,
                                 local_indices_to_remove,
                                 entity_death_info,
                                 ledger_);
    auto const locations{entities.view_locations()};

    for (auto const index : local_indices_to_remove) {
        death_locations_.push_back(vector_at(locations, index));
    }
}
void Sim::publish_deaths() {
    auto const deaths{entity_death_info.get_const_view()};
    auto const death_count{deaths.num()};
    for (std::uint32_t i{}; i < death_count; ++i) {
        ledger_.record_death(deaths.victims[i], deaths.killers[i], deaths.reasons[i]);
    }
}
void Sim::remove_components() {
    entity_tables_.lookups.assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("turrets::Sim::remove_components");

    batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);
    if (local_indices_to_remove.empty()) {
        return;
    }

    entity_tables_.health.remove_rows<EntityType::Turret>(entities.num(), local_indices_to_remove);
}
void Sim::remove_entities() {
    entity_tables_.lookups.assert_removal_allowed();
    SANDBOX_PROFILE_SCOPE("turrets::Sim::remove_entities");

    handle_dead_entities();
    local_indices_to_remove.clear();
    entity_death_info.reset();
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::finish_action");
    profiling::plot("Sandbox/TurretCount", get_num_instances());
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Sim::get_num_instances() const noexcept -> std::uint32_t {
    return entities.num();
}
auto Sim::get_target_ids() const -> std::span<EntityUniqueId const> {
    return entities.get_const_view().target_ids();
}

/* **************************************** */
// Searching
/* **************************************** */
void Sim::perform_search() {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::perform_search");

    auto const n_turrets{get_num_instances()};
    if (n_turrets == 0) {
        return;
    }

    auto const radius{config.detection_radius};

    ml::parallel_for(n_turrets, [this, radius](std::uint32_t const begin, std::uint32_t const end) {
        perform_search_on_slice(begin, end, radius);
    });
}
void Sim::perform_search_on_slice(std::uint32_t const begin,
                                  std::uint32_t const end,
                                  float const radius) {
    std::array<float, 128> candidate_xs;
    std::array<float, 128> candidate_ys;
    std::array<float, 128> candidate_zs;
    ml::FixedArray<LineQueryResult, 128> has_line_of_sight;
    auto const entities{this->entities.get_view()};

    ml::PeriodicTickCountdownView<std::int16_t> const refresh_countdowns{
        entities.target_refresh_countdowns_remaining_ticks(),
        entities.target_refresh_countdowns_periods()};
    auto const current_targets{entities.target_ids()};
    auto const locations{entities.view_locations()};
    auto const turret_teams{entities.teams()};
    auto const fire_point_locations{entities.view_fire_point_locations()};
    auto const integral_biases{entities.integral_biases()};
    ml::FixedArray<EntityUniqueId, 128> target_ids;
    auto const target_capacity{target_ids.capacity_view()};

    for (std::uint32_t i{begin}; i < end; ++i) {
        if (!refresh_countdowns.try_consume(i)) {
            continue;
        }

        if (!current_targets[i].is_valid()) {
            auto const turret_location{vector_at(locations, i)};
            auto const this_team{turret_teams[i]};

            target_ids.set_num_uninitialised(
                spatial_query_manager.collect_non_team_entities_in_range(
                    turret_location, this_team, radius, target_capacity));

            current_targets[i] = EntityUniqueId{};

            auto const target_count{target_ids.num()};
            auto const count{static_cast<std::size_t>(target_count)};
            has_line_of_sight.set_num_uninitialised(target_count);
            // Each turret produces a differently sized candidate batch.
            // NOLINTBEGIN(ioj-loop-view-construction,ioj-loop-view-accessor-call)
            Vectors3fView const candidate_locations_view{std::span{candidate_xs}.first(count),
                                                         std::span{candidate_ys}.first(count),
                                                         std::span{candidate_zs}.first(count)};
            std::array<std::uint32_t, 128> order{};
            std::array<Team, 128> teams{};
            std::array<std::uint8_t, 128> alive{};
            gather_entities(entity_tables_,
                            target_ids,
                            std::span{order}.first(count),
                            {candidate_locations_view,
                             {},
                             std::span{teams}.first(count),
                             std::span{alive}.first(count)});

            spatial_query_manager.has_line_of_sight_to_targets(
                vector_at(fire_point_locations, i),
                candidate_locations_view.get_const_view(),
                target_ids,
                has_line_of_sight);
            // NOLINTEND(ioj-loop-view-construction,ioj-loop-view-accessor-call)

            if (target_count > 0) {
                auto const target_offset{static_cast<std::uint32_t>(
                    integral_biases[i] % static_cast<std::uint32_t>(target_count))};
                auto const loop_bounds{
                    ml::make_rotated_loop_bounds(0, target_count, target_offset)};
                for (auto const bounds : loop_bounds) {
                    for (auto candidate_index{bounds.begin}; candidate_index < bounds.end;
                         ++candidate_index) {
                        auto const element{candidate_index};
                        if (has_line_of_sight[element] == 0) {
                            continue;
                        }

                        auto const candidate{target_ids[element]};
                        if (alive[element] && teams[element] != this_team) {
                            current_targets[i] = candidate;
                            break;
                        }
                    }
                    if (current_targets[i].is_valid()) {
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
void Sim::fire_at_enemies(ml::FrameScratchResource& scratch_resource) {
    SANDBOX_PROFILE_SCOPE("turrets::Sim::fire_at_enemies");

    auto const count{get_num_instances()};
    ml::FrameArray<std::uint32_t> candidate_indices{&scratch_resource};
    ml::FrameArray<EntityUniqueId> hit_ids{&scratch_resource};
    FrameVectors3f starts{scratch_resource};
    FrameVectors3f ends{scratch_resource};
    candidate_indices.reserve(count);
    starts.reserve(count);
    ends.reserve(count);

    auto const entities{this->entities.get_view()};
    auto const disengage_radius{get_disengage_radius()};
    auto const disengage_radius_squared{disengage_radius * disengage_radius};
    auto cooldowns{
        ml::TickCountdownView<std::int16_t>{entities.laser_cooldowns(), cooldown_restart_ticks_}};
    auto const target_ids{entities.target_ids()};
    auto const locations{entities.view_locations()};
    auto const target_locations{entities.view_target_locations()};
    auto const fire_point_locations{entities.view_fire_point_locations()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto& target{target_ids[element]};
        if (!target.is_valid()) {
            continue;
        }
        if (!cooldowns.is_ready(element)) {
            continue;
        }
        if (HMM_LenSqrV3(vector_at(locations, index) - vector_at(target_locations, index)) >=
            disengage_radius_squared) {
            target = {};
            continue;
        }

        candidate_indices.add(index);
        starts.add(vector_at(fire_point_locations, index));
        ends.add(vector_at(target_locations, index));
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

    lasers::FrameSpawnRequests new_lasers{scratch_resource};
    new_lasers.reserve(candidate_count);
    auto const target_velocities{entities.view_target_velocities()};
    auto const laser_damages{entities.laser_damages()};
    auto const entity_ids{entities.entity_ids()};
    auto const teams{entities.teams()};

    for (std::uint32_t candidate{}; candidate < candidate_count; ++candidate) {
        auto const index{candidate_indices[candidate]};
        auto const element{static_cast<std::size_t>(index)};
        if (hit_ids[candidate] != target_ids[element]) {
            continue;
        }

        auto const location{vector_at(fire_point_locations, index)};
        auto const target_location{vector_at(target_locations, index)};
        auto const target_velocity{vector_at(target_velocities, index)};
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
                       laser_damages[element],
                       config.laser.projectile_speed,
                       config.laser.max_distance,
                       entity_ids[element],
                       {teams[element], EntityType::Turret});
    }
    laser_simulation.queue_laser_spawns(new_lasers);
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
} // namespace turrets

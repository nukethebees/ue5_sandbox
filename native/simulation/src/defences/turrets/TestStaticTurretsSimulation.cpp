#include "sandbox/simulation/defences/turrets/TestStaticTurretsSimulation.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <execution>
#include <numeric>
#include <optional>
#include <sandbox/core/countdown.h>
#include <sandbox/core/fixed_array.h>
#include <sandbox/core/periodic_tick_countdown.h>
#include <sandbox/core/tick_countdown.h>
#include <sandbox/simulation/deterministic_bias.h>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <sandbox/simulation/combat/lasers/TestLasersFrameScratch.h>
#include <sandbox/simulation/entities/BatchSimulation.h>
#include <sandbox/simulation/entities/NativeEntityRegistryView.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/profiling.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>
#include <sandbox/simulation/turret_firing.h>
#include <sandbox/simulation/turret_spawn_initialization.h>
#include <sandbox/simulation/turret_targeting.h>

#include <sandbox/core/frame_memory_resource.h>

namespace ml::test_static_turrets {
/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(FTurretSimulationConfig const& new_config) noexcept {
    config = new_config;
}
Simulation::Simulation(FSimulationClock const& clock,
                       FTestEntityRegistry& in_entity_registry,
                       FSpatialQueryManager const& in_spatial_query_manager,
                       ml::test_lasers::Simulation& in_laser_simulation,
                       std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , spatial_query_manager{in_spatial_query_manager}
    , laser_simulation{in_laser_simulation}
    , frame_memory_resource{in_frame_memory_resource} {}

/* **************************************** */
// Spawning
/* **************************************** */
auto Simulation::register_turrets(SpawnDataConstView const spawn_data,
                                  ml::simulation::Rotators3fConstView const rotations)
    -> std::vector<FRegistryEntityHandle> {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::register_turrets");
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
    auto const new_entities_view{entities.get_view(first_new_index, n_to_add)};
    auto const spawn_count{static_cast<std::size_t>(n_to_add)};
    ml::simulation::turrets::initialize_spawned_turrets(
        {.locations = new_entities_view.locations,
         .fire_point_locations = new_entities_view.fire_point_locations,
         .teams = std::as_writable_bytes(std::span{new_entities_view.teams.data(), spawn_count}),
         .healths = {new_entities_view.healths.data(), spawn_count},
         .laser_damages = {new_entities_view.laser_damages.data(), spawn_count},
         .refresh_remaining_ticks = {entities.target_refresh_countdowns_remaining_ticks.data() +
                                         first_new_index,
                                     spawn_count},
         .refresh_periods = {entities.target_refresh_countdowns_periods.data() + first_new_index,
                             spawn_count}},
        spawn_data,
        config.fire_point_offset,
        target_refresh_tick_period,
        target_refresh_next_offset);

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);

    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const rotation{rotations[i]};
        new_entity_data.locations.set(i, spawn_data.locations[i]);
        new_entity_data.rotations.set(i, rotation);
        entities.rotations.set(first_new_index + i, rotation);
    }
    new_entity_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    std::ranges::fill(new_entity_data.entity_types, ml::simulation::EntityType::Turret);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_entity_data.healths[i] = spawn_data.healths[i];
        new_entity_data.teams[i] = spawn_data.teams[i];
        new_entity_data.alive[i] = static_cast<std::uint8_t>(spawn_data.healths[i] > 0);
    }
    auto const new_entities{entity_registry.add_entities(new_entity_data.get_const_view())};
    std::vector<FRegistryEntityHandle> new_handles;
    new_handles.reserve(spawn_count);
    for (std::int32_t i{}; i < n_to_add; ++i) {
        new_handles.push_back(new_entities.get_handle(i));
    }
    for (std::int32_t local_index{}; local_index < n_to_add; ++local_index) {
        entities.handles[first_new_index + local_index] = new_handles[local_index];
    }
    ml::make_deterministic_biases(std::span<FRegistryEntityHandle const>{entities.handles}.subspan(
                                      static_cast<std::size_t>(first_new_index), spawn_count),
                                  std::span<std::uint32_t>{entities.integral_biases}.subspan(
                                      static_cast<std::size_t>(first_new_index), spawn_count));
    validate_array_sizes();
    for (std::int32_t i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.push_back({.kind = EEntityFrameChange::Spawn,
                                  .index = index,
                                  .location = entities.locations[index],
                                  .rotation = entities.rotations[index],
                                  .team = entities.teams[index],
                                  .handle = entities.handles[index]});
    }
    return new_handles;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Simulation::handle_dead_entities() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::handle_dead_entities");
    if (local_indices_to_remove.empty()) {
        return;
    }

    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);

    death_locations_.reserve(local_indices_to_remove.size());
    for (auto const index : local_indices_to_remove) {
        death_locations_.push_back(entities.locations[index]);
        frame_changes_.push_back({.kind = EEntityFrameChange::RemoveSwap,
                                  .index = index,
                                  .handle = entities.handles[index]});
    }
    for (auto const index : local_indices_to_remove) {
        entities.remove_at_swap(index, 1);
    }
}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::begin_play");
    ml::profiling::plot("Sandbox/TestStaticTurretCount", 0);
    assert(search_slice_size > 0);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    assert(cooldown_tick_period >= 0 && std::in_range<std::int16_t>(cooldown_tick_period));
    cooldown_restart_ticks_ = static_cast<std::int16_t>(cooldown_tick_period);
    cooldown_cleaner_ = 0;
    validate_array_sizes();
}
void Simulation::begin_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::begin_tick");
    clear_tick_buffers();
}
void Simulation::update_timers(float const) {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::update_timers");

    ml::tick_countdowns<std::int16_t>(entities.laser_cooldowns, cooldown_cleaner_, 16384);
    ml::tick_periodic_countdowns<std::int16_t>(entities.target_refresh_countdowns_remaining_ticks);
}
void Simulation::make_decisions() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::make_decisions");
    perform_search();
}
void Simulation::queue_commands() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::queue_commands");

    fire_at_enemies();
}
void Simulation::resolve_damage_events() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::resolve_damage_events");

    ml::batch::resolve_damage_events(entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
    validate_array_sizes();
}
void Simulation::update_entity_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::update_entity_registry");

    prepare_entity_update_data();

    entity_registry.queue_entity_updates(
        {
            .indices = entities.handles,
            .data = entity_update_data.get_const_view(),
        },
        entity_death_info);
}
void Simulation::sync_from_registry() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::sync_from_registry");

    entity_registry.refresh_entity_data(entities.target_handles,
                                        entities.target_locations.get_view(),
                                        entities.target_velocities.get_view());

    handle_dead_entities();
}
void Simulation::end_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::end_tick");
    ml::profiling::plot("Sandbox/TestStaticTurretCount", get_num_instances());

    validate_array_sizes();
}

/* **************************************** */
// Entity data
/* **************************************** */
void Simulation::prepare_entity_update_data() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::prepare_entity_update_data");
    entity_update_data.reset();

    auto const n{get_num_instances()};

    entity_update_data.add_uninitialised(n);

    entity_update_data.locations = entities.locations;
    entity_update_data.rotations = entities.rotations;
    entity_update_data.velocities.each_column([](auto& column) { std::ranges::fill(column, 0.f); });
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    std::ranges::fill(entity_update_data.entity_types, ml::simulation::EntityType::Turret);

    for (std::int32_t i{0}; i < n; ++i) {
        entity_update_data.alive[i] = static_cast<std::uint8_t>(entities.healths[i] > 0);
    }
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Simulation::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}
auto Simulation::get_target_handles() const -> std::span<FRegistryEntityHandle const> {
    return entities.target_handles;
}

/* **************************************** */
// Searching
/* **************************************** */
void Simulation::perform_search() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::perform_search");

    auto const n_turrets{get_num_instances()};
    if (n_turrets == 0) {
        return;
    }

    auto const radius{config.detection_radius};

    auto const hardware_thread_count{
        static_cast<std::int32_t>(std::max(1u, std::thread::hardware_concurrency()))};
    auto const max_jobs_for_grain_size{std::max(1, n_turrets / search_slice_size)};
    auto const n_jobs{std::min(hardware_thread_count, max_jobs_for_grain_size)};
    auto const turrets_per_job{(n_turrets + n_jobs - 1) / n_jobs};

    FrameArray<std::int32_t> jobs{&frame_memory_resource};
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
void Simulation::perform_search_on_slice(std::int32_t const job_index,
                                         std::int32_t const n_turrets,
                                         std::int32_t const turrets_per_job,
                                         float const radius) {
    auto const begin{job_index * turrets_per_job};
    auto const end{std::min(begin + turrets_per_job, n_turrets)};
    auto const registry_view{ml::make_native_query_view(entity_registry)};
    std::array<float, 128> candidate_xs;
    std::array<float, 128> candidate_ys;
    std::array<float, 128> candidate_zs;
    ml::FixedArray<std::uint8_t, 128> has_line_of_sight;

    ml::PeriodicTickCountdownView<std::int16_t> const refresh_countdowns{
        entities.target_refresh_countdowns_remaining_ticks,
        entities.target_refresh_countdowns_periods};
    for (std::int32_t i{begin}; i < end; ++i) {
        if (!refresh_countdowns.try_consume(i)) {
            continue;
        }

        if (entities.target_handles[i].is_null()) {
            auto const turret_location{entities.locations[i]};
            auto const this_team{entities.teams[i]};

            ml::FixedArray<FRegistryEntityHandle, 128> target_handles;
            target_handles.set_num_uninitialised(
                spatial_query_manager.collect_non_team_entities_in_range(
                    turret_location, this_team, radius, target_handles.capacity_view()));

            entities.target_handles[i] = FRegistryEntityHandle{};

            auto const target_count{target_handles.num()};
            auto const count{static_cast<std::size_t>(target_count)};
            has_line_of_sight.set_num_uninitialised(target_count);
            ml::simulation::Vectors3fView const candidate_locations_view{
                std::span{candidate_xs}.first(count),
                std::span{candidate_ys}.first(count),
                std::span{candidate_zs}.first(count)};
            for (std::int32_t target_index{}; target_index < target_count; ++target_index) {
                candidate_locations_view.set(
                    target_index, entity_registry.get_location(target_handles[target_index]));
            }

            spatial_query_manager.has_line_of_sight_to_targets(
                entities.fire_point_locations[i],
                candidate_locations_view.get_const_view(),
                target_handles,
                has_line_of_sight);

            entities.target_handles[i] = ml::simulation::select_turret_target(
                {target_handles.data(), static_cast<std::size_t>(target_count)},
                {has_line_of_sight.data(), static_cast<std::size_t>(target_count)},
                registry_view.teams,
                this_team,
                entities.integral_biases[i]);
        }
    }
}

/* **************************************** */
// Attacking
/* **************************************** */
void Simulation::fire_at_enemies() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_static_turrets::Simulation::fire_at_enemies");

    auto const count{static_cast<std::size_t>(get_num_instances())};
    ml::simulation::turrets::FiringView const firing_view{
        .locations = entities.locations.get_const_view(),
        .fire_point_locations = entities.fire_point_locations.get_const_view(),
        .target_locations = entities.target_locations.get_const_view(),
        .target_velocities = entities.target_velocities.get_const_view(),
        .handles = {entities.handles.data(), count},
        .targets = {entities.target_handles.data(), count},
        .laser_damages = {entities.laser_damages.data(), count},
        .teams = std::as_bytes(std::span{entities.teams.data(), count}),
        .cooldowns =
            ml::TickCountdownView<std::int16_t>{entities.laser_cooldowns, cooldown_restart_ticks_}};
    ml::simulation::turrets::FiringScratch scratch{&frame_memory_resource};
    auto const disengage_radius{get_disengage_radius()};
    ml::simulation::turrets::prepare_firing(firing_view,
                                            ml::make_native_query_view(entity_registry),
                                            disengage_radius * disengage_radius,
                                            scratch);
    auto const candidate_count{scratch.candidate_indices.num()};
    if (candidate_count == 0) {
        return;
    }

    spatial_query_manager.trace_line_of_sight(
        scratch.starts.get_const_view(),
        scratch.ends.get_const_view(),
        {scratch.hit_handles.data(), static_cast<std::size_t>(candidate_count)});

    ml::simulation::lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    ml::simulation::turrets::emit_lasers(firing_view,
                                         scratch,
                                         config.laser.projectile_speed,
                                         config.laser.max_distance,
                                         1.e-8f,
                                         new_lasers);
    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}
auto Simulation::get_disengage_radius() const -> float {
    return config.detection_radius * 1.2f;
}

/* **************************************** */
// Misc
/* **************************************** */
void Simulation::clear_tick_buffers() {
    entity_death_info.reset();
    entity_update_data.reset();
    local_indices_to_remove.clear();
}

/* **************************************** */
// Checks
/* **************************************** */
void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
void Simulation::validate_entity_handles() const {
    entity_registry.validate_handles(entities.handles);
}
} // namespace ml::test_static_turrets

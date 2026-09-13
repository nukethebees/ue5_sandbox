#include "sandbox/simulation/combat/lasers/TestLasersSimulation.h"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <sandbox/simulation/laser_collision_response.h>
#include <sandbox/simulation/laser_lifecycle.h>
#include <sandbox/simulation/laser_spawn_initialization.h>
#include <sandbox/simulation/profiling.h>
#include <sandbox/simulation/simulation/SpatialQueryManager.h>

#include <algorithm>
#include <cassert>
#include <execution>
#include <numeric>

namespace ml::test_lasers {
/* **************************************** */
// Construction and tick phases
/* **************************************** */
Simulation::Simulation(FSimulationClock const& clock,
                       FTestEntityRegistry& in_entity_registry,
                       FSpatialQueryManager& in_query_manager,
                       std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : entity_registry{in_entity_registry}
    , query_manager{in_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , simulation_clock{clock} {}

void Simulation::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::begin_play");
    ml::profiling::plot("Sandbox/TestLaserCount", 0);

    number_spawned = 0;
    preallocate_instances();
    validate_array_sizes();
}

void Simulation::begin_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::begin_tick");
}

void Simulation::commit_spawns() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::commit_spawns");
    process_pending_spawns();
    clear_spawn_buffers();
}

void Simulation::simulate(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::simulate");

    ml::simulation::lasers::expire_instances(entities, dt, frame_memory_resource);

    handle_collisions(dt);
    ml::simulation::lasers::update_locations(entities.get_view(), dt);
}

void Simulation::end_tick() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::end_tick");
    ml::profiling::plot("Sandbox/TestLaserCount", get_num_instances());
    validate_array_sizes();
}

auto Simulation::get_num_instances() const noexcept -> std::int32_t {
    return entities.num();
}

/* **************************************** */
// Spawning
/* **************************************** */
void Simulation::queue_laser_spawns(
    ml::simulation::lasers::SpawnRequestsConstView const spawn_data) {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::queue_laser_spawns");

    spawn_data.validate_array_sizes();
    pending_spawns.append_from(spawn_data);
}

void Simulation::preallocate_instances() {
    entities.reserve(n_preallocated_instances);
}

void Simulation::process_pending_spawns() {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::process_pending_spawns");

    pending_spawns.validate_array_sizes();
    auto const n_to_add{pending_spawns.num()};
    entity_registry.record_shots(pending_spawns.instigator_handles);

    if (n_to_add <= 0) {
        return;
    }

    auto const tick_period{static_cast<float>(simulation_clock.get_tick_period())};
    constexpr float fixed_spawn_offset{10.f};
    auto const time{static_cast<float>(simulation_clock.get_simulation_time())};
    ml::simulation::lasers::initialise_spawns(
        entities, pending_spawns.get_const_view(), tick_period, time, fixed_spawn_offset);

    number_spawned += n_to_add;
    validate_array_sizes();
}

/* **************************************** */
// Movement and collision
/* **************************************** */
void Simulation::handle_collisions(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::test_lasers::Simulation::handle_collisions");

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    FrameCollisionScratch collision_scratch{&frame_memory_resource};
    collision_scratch.set_num(n);
    auto const locations{entities.locations.get_const_view()};
    auto const velocities{entities.velocities.get_const_view()};
    assert(collision_jobs > 0);
    auto const job_count{std::min(n, collision_jobs)};
    auto const updates_per_slice{n / job_count + (n % job_count != 0)};
    FrameArray<std::int32_t> jobs{&frame_memory_resource};
    jobs.set_num(job_count);
    auto const job_indices{jobs.view()};
    std::iota(job_indices.begin(), job_indices.end(), 0);
    std::for_each(
        std::execution::par,
        job_indices.begin(),
        job_indices.end(),
        [=, this, &collision_scratch](std::int32_t const job_index) {
            auto const i_start{job_index * updates_per_slice};
            auto const i_end{std::min(i_start + updates_per_slice, n)};
            auto const trace_count{i_end - i_start};
            if (trace_count <= 0) {
                return;
            }

            ml::simulation::lasers::prepare_collision_traces(
                locations.slice(i_start, trace_count),
                velocities.slice(i_start, trace_count),
                dt,
                collision_scratch.trace_starts.get_view().slice(i_start, trace_count),
                collision_scratch.trace_ends.get_view().slice(i_start, trace_count));

            auto const traces{ml::simulation::LineTracesConstView{
                collision_scratch.trace_starts.get_const_view().slice(i_start, trace_count),
                collision_scratch.trace_ends.get_const_view().slice(i_start, trace_count)}};
            auto const hits{collision_scratch.trace_hits.get_view().slice(i_start, trace_count)};
            auto const ignored_entities{
                std::span<FRegistryEntityHandle const>{entities.instigator_handles}.subspan(
                    i_start, trace_count)};
            query_manager.get_collision_system().get_uniform_grid().trace_aabbs(
                traces, hits, ignored_entities);
        });

    FrameArray<std::int32_t> to_remove{&frame_memory_resource};
    FrameHitDetails hit_details{&frame_memory_resource};
    FrameDirectDamageEvents collision_damage_events{&frame_memory_resource};
    ml::simulation::lasers::process_collision_hits(
        collision_scratch.trace_hits.get_const_view(),
        entities.velocities.get_const_view(),
        {entities.damages.data(), static_cast<std::size_t>(n)},
        {entities.instigator_handles.data(), static_cast<std::size_t>(n)},
        {entities.sources.data(), static_cast<std::size_t>(n)},
        to_remove,
        collision_damage_events,
        hit_details);
    entity_registry.queue_direct_damage_events(collision_damage_events.get_const_view());

    ml::simulation::lasers::remove_instances(entities, to_remove.view());

    frame_output_.append_hits(hit_details.get_const_view(), simulation_clock.get_completed_ticks());
}

/* **************************************** */
// Buffer cleanup and validation
/* **************************************** */
void Simulation::clear_spawn_buffers() {
    pending_spawns.reset();
}

void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
} // namespace ml::test_lasers

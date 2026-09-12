#include "SpaceGameSimulation/combat/lasers/TestLasersSimulation.h"

#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/LineTraces.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <Async/ParallelFor.h>
#include <ProfilingDebugging/CountersTrace.h>
#include <Templates/Greater.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestLaserCount, TEXT("Sandbox/TestLaserCount"));

namespace ml::test_lasers {
/* **************************************** */
// Spawn request configuration
/* **************************************** */
void SpawnRequests::set_damages(int32 const value) {
    ml::fill(damages, value);
}

void SpawnRequests::set_speeds(float const value) {
    ml::fill(speeds, value);
}

void SpawnRequests::set_max_distances(float const value) {
    ml::fill(max_distances, value);
}

void SpawnRequests::set_sources(simulation::LaserSource const value) {
    ml::fill(sources, value);
}

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
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);

    number_spawned = 0;
    preallocate_instances();
    validate_array_sizes();
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::begin_tick);
}

void Simulation::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::commit_spawns);
    process_pending_spawns();
    clear_spawn_buffers();
}

void Simulation::simulate(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::simulate);

    TFrameArray<int32> expired_indices{&frame_memory_resource};
    tick_lifetimes(dt);
    collect_old_instance_indices(expired_indices);
    remove_instances(expired_indices);

    handle_collisions(dt);
    update_locations(dt);
}

void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::end_tick);
    TRACE_COUNTER_SET(SandboxTestLaserCount, get_num_instances());
    validate_array_sizes();
}

auto Simulation::get_num_instances() const noexcept -> int32 {
    return entities.lifetimes_remaining.Num();
}

/* **************************************** */
// Spawning
/* **************************************** */
void Simulation::queue_laser_spawns(SpawnRequestsConstView const spawn_data) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::queue_laser_spawns);

    spawn_data.validate_array_sizes();
    pending_spawns.append_from(spawn_data);
}

void Simulation::preallocate_instances() {
    entities.reserve(n_preallocated_instances);
}

void Simulation::process_pending_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::process_pending_spawns);

    pending_spawns.validate_array_sizes();
    auto const n_to_add{ml::num(pending_spawns)};
    entity_registry.record_shots(pending_spawns.instigator_handles);

    if (n_to_add <= 0) {
        return;
    }

    auto const tick_period{static_cast<float>(simulation_clock.get_tick_period())};
    constexpr float fixed_spawn_offset{10.f};
    auto const offset{get_num_instances()};

    ml::append_from(entities.sources, pending_spawns.sources);
    ml::append_from(entities.locations, pending_spawns.locations);
    ml::append_from(entities.rotations, pending_spawns.rotations);
    entities.damages.Append(pending_spawns.damages);
    entities.instigator_handles.Append(pending_spawns.instigator_handles);

    ml::add_uninitialised(n_to_add,
                          entities.velocities,
                          entities.lifetimes_remaining,
                          entities.initial_lifetimes,
                          entities.spawn_times);

    auto const time{static_cast<float>(simulation_clock.get_simulation_time())};
    for (int32 i{0}; i < n_to_add; ++i) {
        auto const speed{pending_spawns.speeds[i]};
        auto const max_distance{pending_spawns.max_distances[i]};
        auto const lifetime{max_distance / speed};
        auto const index{offset + i};

        auto const forward_direction{ml::get_rotator3f(entities.rotations, index).Vector()};
        auto const forward_velocity{forward_direction * speed};
        auto const base_velocity{ml::get_vector3f(pending_spawns.base_velocities, i)};
        auto const velocity{base_velocity + forward_velocity};
        auto const base_spawn_location{ml::get_vector3f(entities.locations, index)};
        auto const spawn_location{base_spawn_location + forward_velocity * tick_period +
                                  forward_direction * fixed_spawn_offset};

        entities.locations.set(index, spawn_location);
        entities.velocities.set(index, velocity);
        entities.lifetimes_remaining[index] = lifetime;

        entities.initial_lifetimes[index] = lifetime;
        entities.spawn_times[index] = time;
    }

    number_spawned += n_to_add;
    validate_array_sizes();
}

/* **************************************** */
// Movement and collision
/* **************************************** */
void Simulation::update_locations(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::update_locations);
    ml::add_scaled_in_place(entities.locations, entities.velocities, dt);
}

void Simulation::handle_collisions(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::handle_collisions);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    FrameCollisionScratch collision_scratch{&frame_memory_resource};
    collision_scratch.set_num(n);
    auto const updates_per_slice{FMath::DivideAndRoundUp(n, collision_jobs)};
    ParallelFor(collision_jobs, [=, this, &collision_scratch](int32 const job_index) {
        auto const i_start{job_index * updates_per_slice};
        auto const i_end{FMath::Min(i_start + updates_per_slice, n)};
        auto const trace_count{i_end - i_start};
        if (trace_count <= 0) {
            return;
        }

        for (int32 trace_index{i_start}; trace_index < i_end; ++trace_index) {
            auto const start{ml::get_vector3f(entities.locations, trace_index)};
            auto const velocity{ml::get_vector3f(entities.velocities, trace_index)};
            collision_scratch.trace_starts.set(trace_index, ml::to_native(start));
            collision_scratch.trace_ends.set(trace_index, ml::to_native(start + dt * velocity));
        }

        auto const traces{make_line_traces_const_view(
            collision_scratch.trace_starts.get_const_view().slice(i_start, trace_count),
            collision_scratch.trace_ends.get_const_view().slice(i_start, trace_count))};
        auto const hits{collision_scratch.trace_hits.get_view().slice(i_start, trace_count)};
        auto const ignored_entities{
            TConstArrayView<FRegistryEntityHandle>{entities.instigator_handles}.Slice(i_start,
                                                                                      trace_count)};
        query_manager.get_collision_system().get_uniform_grid().trace_aabbs(
            traces, hits, ignored_entities);
    });

    TFrameArray<int32> to_remove{&frame_memory_resource};
    FrameHitDetails hit_details{&frame_memory_resource};
    FrameDirectDamageEvents collision_damage_events{&frame_memory_resource};

    int32 detected_hit_count{};
    for (auto const hit : collision_scratch.trace_hits.hits) {
        detected_hit_count += hit != 0 ? 1 : 0;
    }
    to_remove.reserve(detected_hit_count);
    hit_details.reserve(detected_hit_count);
    collision_damage_events.reserve(detected_hit_count);

    for (int32 entity_index{}; entity_index < n; ++entity_index) {
        if (collision_scratch.trace_hits.hits[entity_index] == 0) {
            continue;
        }

        to_remove.add(entity_index);

        auto const damaged_entity{collision_scratch.trace_hits.entities[entity_index]};
        if (damaged_entity.is_valid()) {
            collision_damage_events.add(damaged_entity,
                                        entities.damages[entity_index],
                                        entities.instigator_handles[entity_index]);
        }

        auto const velocity{ml::get_vector3f(entities.velocities, entity_index)};
        hit_details.add(
            collision_scratch.trace_hits.locations.get_const_view()[entity_index],
            ml::to_native(-velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector)),
            entities.sources[entity_index]);
    }
    entity_registry.queue_direct_damage_events(collision_damage_events.get_const_view());

    to_remove.view().Sort(TGreater<int32>{});
    remove_instances(to_remove);

    frame_hits_.append_from(make_hit_details_const_view(hit_details));

    auto const hit_count{hit_details.num()};
    for (int32 i{}; i < hit_count; ++i) {
        hit_ticks_.Add(simulation_clock.get_completed_ticks());
        hit_ordinals_.Add(i);
    }
}

/* **************************************** */
// Lifetime and removal
/* **************************************** */
void Simulation::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::tick_lifetimes);
    ml::subtract_in_place(TArrayView<float>{entities.lifetimes_remaining}, dt);
}

void Simulation::collect_old_instance_indices(TFrameArray<int32>& indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::collect_old_instance_indices);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    indices.reserve(n);
    for (int32 i{n - 1}; i >= 0; --i) {
        if (entities.lifetimes_remaining[i] <= 0.f) {
            indices.add(i);
        }
    }
}

void Simulation::remove_instances(TConstArrayView<int32> const indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    ml::remove_at_swap_many_sorted_desc(indices, entities);
    validate_array_sizes();
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

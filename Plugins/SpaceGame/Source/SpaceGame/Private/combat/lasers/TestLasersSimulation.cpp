#include "SpaceGame/combat/lasers/TestLasersSimulation.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>

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
void SpawnRequests::set_damages(int32 const value) {
    ml::fill(damages, value);
}

void SpawnRequests::set_speeds(float const value) {
    ml::fill(speeds, value);
}

void SpawnRequests::set_max_distances(float const value) {
    ml::fill(max_distances, value);
}

void SpawnRequests::set_colours(FLinearColor const value) {
    ml::fill(colours, value);
}

void Simulation::bind_simulation_clock(FSimulationClock const& clock) noexcept {
    simulation_clock.bind(clock);
}

void Simulation::set_entity_registry(FTestEntityRegistry& new_entity_registry) noexcept {
    entity_registry = &new_entity_registry;
}

void Simulation::set_spatial_query_manager(FSpatialQueryManager& new_query_manager) noexcept {
    query_manager = &new_query_manager;
}

void Simulation::clear_runtime_state() {
    ml::reset(entities);
    clear_spawn_buffers();
    clear_hit_buffers();
    clear_presentation_events();
}

void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestLaserCount, 0);
    check(entity_registry);
    check(query_manager);
    check(simulation_clock.is_valid());

    number_spawned = 0;
    preallocate_instances();
    validate_array_sizes();
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::begin_tick);
    clear_hit_buffers();
    clear_presentation_events();
}

void Simulation::commit_spawns() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::commit_spawns);
    process_pending_spawns();
    clear_spawn_buffers();
}

void Simulation::simulate(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::simulate);

    tick_lifetimes(dt);
    collect_old_instance_indices();
    remove_instances(to_remove);

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

void Simulation::queue_laser_spawns(SpawnRequests const& spawn_data) {
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
    if (n_to_add <= 0) {
        return;
    }

    auto const tick_period{static_cast<float>(simulation_clock.get_tick_period())};
    constexpr float fixed_spawn_offset{10.f};

    auto const offset{get_num_instances()};

    presentation_spawn_count = n_to_add;
    presentation_custom_data_to_add.SetNumUninitialized(n_to_add * 5, EAllowShrinking::No);

    ml::append_from(entities.colours, pending_spawns.colours);
    ml::append_from(entities.locations, pending_spawns.locations);
    ml::append_from(entities.rotations, pending_spawns.rotations);
    entities.damages.Append(pending_spawns.damages);
    entities.instigator_handles.Append(pending_spawns.instigator_handles);

    ml::add_uninitialised(n_to_add, entities.velocities, entities.lifetimes_remaining);

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

        ml::assign(entities.locations, index, spawn_location);
        ml::assign(entities.velocities, index, velocity);
        entities.lifetimes_remaining[index] = lifetime;

        auto const base{i * 5};
        auto const& colour{pending_spawns.colours[i]};
        presentation_custom_data_to_add[base + 0] = colour.R;
        presentation_custom_data_to_add[base + 1] = colour.G;
        presentation_custom_data_to_add[base + 2] = colour.B;
        presentation_custom_data_to_add[base + 3] = lifetime;
        presentation_custom_data_to_add[base + 4] = time;
    }

    number_spawned += n_to_add;
    validate_array_sizes();
}

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

    auto& data{thread_local_collision_data};
    if (data.Num() < collision_jobs) {
        data.SetNum(collision_jobs);
    }

    auto const updates_per_slice{FMath::DivideAndRoundUp(n, collision_jobs)};
    ParallelFor(collision_jobs, [=, this](int32 const job_index) {
        check_collision_thread(
            job_index, updates_per_slice, dt, thread_local_collision_data[job_index], *this);
    });

    merge_collision_data();
    entity_registry->queue_direct_damage_events(collision_damage_events);

    to_remove.Sort(TGreater<int32>{});
    remove_instances(to_remove);
    hit_details.validate_array_sizes();
}

void Simulation::check_collision_thread(int32 const job_index,
                                        int32 const updates_per_slice,
                                        float const dt,
                                        ThreadLocalCollisionData& data,
                                        Simulation const& simulation) {
    auto const n{simulation.get_num_instances()};
    ml::reset(data.traces, data.trace_hits, data.damage_events, data.to_remove, data.hit_details);

    auto const i_start{job_index * updates_per_slice};
    auto const i_end{FMath::Min(i_start + updates_per_slice, n)};
    auto const trace_count{i_end - i_start};
    if (trace_count <= 0) {
        return;
    }

    data.traces.add_uninitialised(trace_count);
    data.trace_hits.add_defaulted(trace_count);

    for (int32 trace_index{}; trace_index < trace_count; ++trace_index) {
        auto const entity_index{i_start + trace_index};
        auto const start{ml::get_vector3f(simulation.entities.locations, entity_index)};
        auto const velocity{ml::get_vector3f(simulation.entities.velocities, entity_index)};
        data.traces.starts.set(trace_index, start);
        data.traces.ends.set(trace_index, start + dt * velocity);
    }

    auto const ignored_entities{
        TConstArrayView<FRegistryEntityHandle>{simulation.entities.instigator_handles}.Slice(
            i_start, trace_count)};
    simulation.query_manager->get_collision_system().get_uniform_grid().trace_aabbs(
        data.traces.get_const_view(), data.trace_hits.get_view(), ignored_entities);

    for (int32 trace_index{}; trace_index < trace_count; ++trace_index) {
        if (data.trace_hits.hits[trace_index] == 0) {
            continue;
        }

        auto const entity_index{i_start + trace_index};
        data.to_remove.Add(entity_index);

        auto const damaged_entity{data.trace_hits.entities[trace_index]};
        if (damaged_entity.is_valid()) {
            data.damage_events.damaged_entities.Add(damaged_entity);
            data.damage_events.damage_amounts.Add(simulation.entities.damages[entity_index]);
            data.damage_events.instigators.Add(
                simulation.entities.instigator_handles[entity_index]);
        }

        data.hit_details.locations.add(data.trace_hits.locations[trace_index]);
    }
}

void Simulation::merge_collision_data() {
    auto& data{thread_local_collision_data};
    ml::reset(to_remove, hit_details, collision_damage_events);

    for (int32 i{}; i < collision_jobs; ++i) {
        auto const& thread_data{data[i]};
        auto const n_hits{thread_data.to_remove.Num()};
        check(n_hits == ml::num(thread_data.hit_details.locations));

        for (int32 j{}; j < n_hits; ++j) {
            auto const entity_index{thread_data.to_remove[j]};
            to_remove.Add(entity_index);
            hit_details.locations.add(thread_data.hit_details.locations[j]);
            hit_details.colours.Add(entities.colours[entity_index]);
        }

        auto const damage_count{thread_data.damage_events.num()};
        for (int32 j{}; j < damage_count; ++j) {
            collision_damage_events.damaged_entities.Add(
                thread_data.damage_events.damaged_entities[j]);
            collision_damage_events.damage_amounts.Add(thread_data.damage_events.damage_amounts[j]);
            collision_damage_events.instigators.Add(thread_data.damage_events.instigators[j]);
        }
    }
}

void Simulation::tick_lifetimes(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::tick_lifetimes);
    ml::subtract_in_place(TArrayView<float>{entities.lifetimes_remaining}, dt);
}

void Simulation::collect_old_instance_indices() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::collect_old_instance_indices);

    auto const n{get_num_instances()};
    if (n < 1) {
        return;
    }

    to_remove.Reset();
    for (int32 i{n - 1}; i >= 0; --i) {
        if (entities.lifetimes_remaining[i] <= 0.f) {
            to_remove.Add(i);
        }
    }
}

void Simulation::remove_instances(TConstArrayView<int32> const indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_lasers::Simulation::remove_instances);

    auto const n{indices.Num()};
    if (n < 1) {
        return;
    }

    presentation_indices_to_remove.Append(indices.GetData(), n);
    ml::remove_at_swap_many_sorted_desc(indices, entities);
    validate_array_sizes();
}

void Simulation::clear_spawn_buffers() {
    ml::reset(pending_spawns, to_remove);
}

void Simulation::clear_hit_buffers() {
    ml::reset(hit_details, collision_damage_events);
}

void Simulation::clear_presentation_events() {
    presentation_indices_to_remove.Reset();
    presentation_custom_data_to_add.Reset();
    presentation_spawn_count = 0;
}

void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
} // namespace ml::test_lasers

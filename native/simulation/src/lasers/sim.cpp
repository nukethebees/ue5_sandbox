#include "ioj/sim/lasers/sim.h"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <ioj/sim/entity_registry.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <functional>
#include <numeric>
#include <sandbox/core/frame_array.h>
#include <sandbox/core/generated/array_math_kernels.h>

namespace ioj::sim::lasers {
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
void append_spawn_requests(SingleAllocationLaserSpawnRequests& destination,
                           SpawnRequestsConstView const source) {
    auto const old_count{destination.num()};
    destination.add_uninitialised(source.num());
    auto const output{destination.get_view().columns().get_view(old_count, source.num())};
    copy_vectors(output.locations, source.locations);
    copy_rotators(output.rotations, source.rotations);
    copy_vectors(output.base_velocities, source.base_velocities);
    std::ranges::copy(source.damages, output.damages.begin());
    std::ranges::copy(source.speeds, output.speeds.begin());
    std::ranges::copy(source.max_distances, output.max_distances.begin());
    std::ranges::copy(source.instigator_ids, output.instigator_ids.begin());
    std::ranges::copy(source.sources, output.sources.begin());
}
} // namespace

/* **************************************** */
// Construction and tick phases
/* **************************************** */
Sim::Sim(SimClock const& clock,
         EntityRegistry& in_entity_registry,
         SpatialQueryManager& in_query_manager,
         std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : entity_registry{in_entity_registry}
    , query_manager{in_query_manager}
    , frame_memory_resource{in_frame_memory_resource}
    , simulation_clock{clock} {}

void Sim::set_config(LaserSimConfig const& new_config) noexcept {
    config = new_config;
}

void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::begin_play");
    profiling::plot("Sandbox/LaserCount", 0);

    number_spawned = 0;
    preallocate_instances();
    validate_array_sizes();
}

void Sim::commit_spawns() {
    assert(simulation_clock.permits_preparation_mutation());
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::commit_spawns");
    process_pending_spawns();
    clear_spawn_buffers();
}

void Sim::cleanup_entities() {
    assert(simulation_clock.phase == SimulationPhase::ResolutionCommit);
    std::ranges::sort(pending_removals_, std::greater{});
    auto const duplicate{std::ranges::unique(pending_removals_)};
    pending_removals_.erase(duplicate.begin(), duplicate.end());
    remove_instances(pending_removals_);
    pending_removals_.clear();
}

void Sim::simulate(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::simulate");

    handle_collisions(dt);
    update_locations(dt);
    expire_instances(dt);
}

void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::finish_action");
    profiling::plot("Sandbox/LaserCount", get_num_instances());
    validate_array_sizes();
}

auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return static_cast<std::int32_t>(
        std::ranges::count(entities.get_const_view().active(), std::uint8_t{1}));
}

/* **************************************** */
// Spawning
/* **************************************** */
void Sim::queue_laser_spawns(lasers::SpawnRequestsConstView const spawn_data) {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::queue_laser_spawns");

    spawn_data.validate_array_sizes();
    append_spawn_requests(pending_spawns, spawn_data);
}

void Sim::preallocate_instances() {
    entities.reserve(config.n_preallocated_instances);
}

void Sim::process_pending_spawns() {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::process_pending_spawns");

    pending_spawns.get_const_view().columns().validate_array_sizes();
    auto const n_to_add{pending_spawns.num()};
    entity_registry.record_shots(pending_spawns.get_const_view().instigator_ids());

    if (n_to_add <= 0) {
        return;
    }

    auto const requests{pending_spawns.get_const_view().columns()};
    auto const simulation_time{static_cast<float>(simulation_clock.get_simulation_time())};
    constexpr float fixed_spawn_offset{10.f};
    entities.add_defaulted(n_to_add);
    auto const output{entities.get_view().right(n_to_add).columns()};
    for (std::int32_t spawn_index{}; spawn_index < n_to_add; ++spawn_index) {
        output.active[spawn_index] = 1;
        auto const speed{requests.speeds[spawn_index]};
        auto const lifetime{requests.max_distances[spawn_index] / speed};
        auto const rotation{requests.rotations[spawn_index]};
        auto const pitch{HMM_AngleDeg(rotation.pitch)};
        auto const yaw{HMM_AngleDeg(rotation.yaw)};
        auto const cos_pitch{HMM_CosF(pitch)};
        auto const direction{
            HMM_V3(cos_pitch * HMM_CosF(yaw), cos_pitch * HMM_SinF(yaw), HMM_SinF(pitch))};
        auto const forward_velocity{direction * speed};

        output.locations.set(spawn_index,
                             requests.locations[spawn_index] + direction * fixed_spawn_offset);
        output.rotations.set(spawn_index, rotation);
        output.velocities.set(spawn_index,
                              requests.base_velocities[spawn_index] + forward_velocity);
        output.sources[spawn_index] = requests.sources[spawn_index];
        output.damages[spawn_index] = requests.damages[spawn_index];
        output.instigator_ids[spawn_index] = requests.instigator_ids[spawn_index];
        output.lifetimes_remaining[spawn_index] = lifetime;
        output.initial_lifetimes[spawn_index] = lifetime;
        output.spawn_times[spawn_index] = simulation_time;
    }

    number_spawned += n_to_add;
    validate_array_sizes();
}

/* **************************************** */
// Movement and collision
/* **************************************** */
void Sim::expire_instances(float const dt) {
    auto const entities{this->entities.get_view().columns()};
    ml::subtract_in_place(std::span<float>{entities.lifetimes_remaining}, dt);

    ml::FrameArray<std::int32_t> expired_indices{&frame_memory_resource};
    auto const count{entities.num()};
    expired_indices.reserve(count);
    for (std::int32_t index{count - 1}; index >= 0; --index) {
        if (entities.active[index] != 0 && entities.lifetimes_remaining[index] <= 0.f) {
            entities.active[index] = 0;
            expired_indices.add(index);
        }
    }
    pending_removals_.insert(
        pending_removals_.end(), expired_indices.begin(), expired_indices.end());
}
void Sim::update_locations(float const dt) {
    auto const entities{this->entities.get_view().columns()};
    auto const count{entities.num()};
    for (std::int32_t index{}; index < count; ++index) {
        if (entities.active[index] != 0) {
            auto const step{std::min(dt, std::max(0.f, entities.lifetimes_remaining[index]))};
            entities.locations.set(index,
                                   entities.locations[index] + entities.velocities[index] * step);
        }
    }
}
void Sim::handle_collisions(float const dt) {
    SANDBOX_PROFILE_SCOPE("Sandbox::lasers::Sim::handle_collisions");

    auto const n{entities.num()};
    if (n < 1) {
        return;
    }

    FrameCollisionScratch collision_scratch{&frame_memory_resource};
    collision_scratch.set_num(n);
    auto const entities{this->entities.get_const_view().columns()};
    auto const locations{entities.locations.get_const_view()};
    auto const velocities{entities.velocities.get_const_view()};
    assert(config.collision_jobs > 0);
    auto const job_count{std::min(n, config.collision_jobs)};
    auto const updates_per_slice{n / job_count + (n % job_count != 0)};
    ml::FrameArray<std::int32_t> jobs{&frame_memory_resource};
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

            auto const trace_locations{locations.slice(i_start, trace_count)};
            auto const trace_velocities{velocities.slice(i_start, trace_count)};
            auto const trace_starts{
                collision_scratch.trace_starts.get_view().slice(i_start, trace_count)};
            auto const trace_ends{
                collision_scratch.trace_ends.get_view().slice(i_start, trace_count)};
            for (std::int32_t trace_index{}; trace_index < trace_count; ++trace_index) {
                auto const start{trace_locations[trace_index]};
                trace_starts.set(trace_index, start);
                auto const index{i_start + trace_index};
                auto const step{
                    entities.active[index] != 0
                        ? std::min(dt, std::max(0.f, entities.lifetimes_remaining[index]))
                        : 0.f};
                trace_ends.set(trace_index, start + trace_velocities[trace_index] * step);
            }

            auto const traces{LineTracesConstView{
                collision_scratch.trace_starts.get_const_view().slice(i_start, trace_count),
                collision_scratch.trace_ends.get_const_view().slice(i_start, trace_count)}};
            auto const hits{collision_scratch.trace_hits.get_view().slice(i_start, trace_count)};
            auto const ignored_entities{
                std::span<EntityUniqueId const>{entities.instigator_ids}.subspan(i_start,
                                                                                 trace_count)};
            query_manager.get_collision_system().get_uniform_grid().trace_aabbs(
                traces, hits, ignored_entities);
        });

    ml::FrameArray<std::int32_t> to_remove{&frame_memory_resource};
    FrameHitDetails hit_details{&frame_memory_resource};
    FrameDirectDamageEvents collision_damage_events{&frame_memory_resource};
    auto const trace_hits{collision_scratch.trace_hits.get_const_view()};
    auto const hit_count{static_cast<std::int32_t>(
        std::ranges::count_if(trace_hits.hits, [](auto const hit) { return hit != 0; }))};
    to_remove.reserve(hit_count);
    collision_damage_events.reserve(hit_count);
    hit_details.reserve(hit_count);
    constexpr float safe_normal_tolerance{1.e-8f};
    for (std::int32_t entity_index{}; entity_index < n; ++entity_index) {
        auto const element{static_cast<std::size_t>(entity_index)};
        if (entities.active[element] == 0 || trace_hits.hits[element] == 0) {
            continue;
        }

        to_remove.add(entity_index);
        auto const damaged_entity{trace_hits.entities[element]};
        if (damaged_entity.is_valid()) {
            auto const instigator{entities.instigator_ids[element]};
            collision_damage_events.add(entity_registry.get_current_id(damaged_entity),
                                        entities.damages[element],
                                        instigator);
        }

        auto const velocity{entities.velocities[entity_index]};
        auto const length_squared{HMM_LenSqrV3(velocity)};
        auto const emission_direction{length_squared < safe_normal_tolerance
                                          ? HMM_V3(0.f, 0.f, -1.f)
                                          : velocity * (-1.f / std::sqrt(length_squared))};
        hit_details.add(
            trace_hits.locations[entity_index], emission_direction, entities.sources[element]);
    }
    std::ranges::sort(to_remove.view(), std::greater{});
    entity_registry.queue_direct_damage_events(collision_damage_events.get_const_view());

    auto const active{this->entities.get_view().active()};
    for (auto const index : to_remove) {
        active[index] = 0;
        pending_removals_.push_back(index);
    }

    frame_output_.append_hits(hit_details.get_const_view(), simulation_clock.get_completed_ticks());
}
void Sim::remove_instances(std::span<std::int32_t const> const indices) {
    assert(std::ranges::is_sorted(indices, std::greater{}));
    for (auto const index : indices) {
        entities.remove_at_swap(index, 1);
    }
    validate_array_sizes();
}

/* **************************************** */
// Buffer cleanup and validation
/* **************************************** */
void Sim::clear_spawn_buffers() {
    pending_spawns.reset();
}

void Sim::validate_array_sizes() const {
    entities.get_const_view().columns().validate_array_sizes();
}
} // namespace lasers

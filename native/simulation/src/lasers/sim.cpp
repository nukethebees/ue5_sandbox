#include "ioj/sim/lasers/sim.h"

#include <ioj/sim/column_math.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/spatial_query_manager.h>

#include <sandbox/core/frame_array.h>
#include <sandbox/core/generated/array_math_kernels.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <execution>
#include <functional>
#include <numeric>
#include <span>
#include <vector>

namespace ioj::sim::lasers {

/* **************************************** */
// Construction and tick phases
/* **************************************** */
Sim::Sim(SimClock const& clock,
         CombatEvents& in_combat_events,
         SpatialQueryManager& in_query_manager) noexcept
    : combat_events{in_combat_events}
    , query_manager{in_query_manager}
    , simulation_clock{clock} {}

void Sim::set_config(LaserSimConfig const& new_config) noexcept {
    config = new_config;
}

void Sim::begin_play() {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::begin_play");
    profiling::plot("Sandbox/LaserCount", 0);

    number_spawned = 0;
    preallocate_instances();
}

void Sim::commit_spawns() {
    assert(simulation_clock.permits_preparation_mutation());
    SANDBOX_PROFILE_SCOPE("lasers::Sim::commit_spawns");
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

void Sim::simulate(float const dt, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::simulate");

    handle_collisions(dt, scratch);
    update_locations(dt);
    expire_instances(dt, scratch);
}

void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::finish_action");
    profiling::plot("Sandbox/LaserCount", get_num_instances());
}

auto Sim::get_num_instances() const noexcept -> std::int32_t {
    return static_cast<std::int32_t>(
        std::ranges::count(entities.get_const_view().active(), std::uint8_t{1}));
}

/* **************************************** */
// Spawning
/* **************************************** */

void Sim::preallocate_instances() {
    entities.reserve(config.n_preallocated_instances);
}

void Sim::process_pending_spawns() {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::process_pending_spawns");

    pending_spawns.get_const_view().validate();
    auto const n_to_add{pending_spawns.num()};
    combat_events.record_shots(pending_spawns.get_const_view().instigator_ids());

    if (n_to_add <= 0) {
        return;
    }

    auto const requests{pending_spawns.get_const_view()};
    auto const simulation_time{static_cast<float>(simulation_clock.get_simulation_time())};
    constexpr float fixed_spawn_offset{10.f};
    entities.add_uninitialised(n_to_add);
    auto const output{entities.get_view().right(n_to_add)};
    auto const output_active{output.active()};
    auto const output_locations{output.view_locations()};
    auto const output_rotations{output.view_rotations()};
    auto const output_velocities{output.view_velocities()};
    auto const output_sources{output.sources()};
    auto const output_damages{output.damages()};
    auto const output_instigator_ids{output.instigator_ids()};
    auto const output_lifetimes_remaining{output.lifetimes_remaining()};
    auto const output_initial_lifetimes{output.initial_lifetimes()};
    auto const output_spawn_times{output.spawn_times()};

    auto const requests_speeds{requests.speeds()};
    auto const requests_max_distances{requests.max_distances()};
    auto const requests_rotations{requests.view_rotations()};
    auto const requests_locations{requests.view_locations()};
    auto const requests_base_velocities{requests.view_base_velocities()};
    auto const requests_sources{requests.sources()};
    auto const requests_damages{requests.damages()};
    auto const requests_instigator_ids{requests.instigator_ids()};

    for (std::int32_t spawn_index{}; spawn_index < n_to_add; ++spawn_index) {
        output_active[spawn_index] = 1;
        auto const speed{requests_speeds[spawn_index]};
        auto const lifetime{requests_max_distances[spawn_index] / speed};
        auto const rotation{rotation_at(requests_rotations, spawn_index)};
        auto const pitch{HMM_AngleDeg(rotation.pitch)};
        auto const yaw{HMM_AngleDeg(rotation.yaw)};
        auto const cos_pitch{HMM_CosF(pitch)};
        auto const direction{
            HMM_V3(cos_pitch * HMM_CosF(yaw), cos_pitch * HMM_SinF(yaw), HMM_SinF(pitch))};
        auto const forward_velocity{direction * speed};

        set_vector(output_locations,
                   spawn_index,
                   vector_at(requests_locations, spawn_index) + direction * fixed_spawn_offset);
        set_rotation(output_rotations, spawn_index, rotation);
        set_vector(output_velocities,
                   spawn_index,
                   vector_at(requests_base_velocities, spawn_index) + forward_velocity);
        output_sources[spawn_index] = requests_sources[spawn_index];
        output_damages[spawn_index] = requests_damages[spawn_index];
        output_instigator_ids[spawn_index] = requests_instigator_ids[spawn_index];
        output_lifetimes_remaining[spawn_index] = lifetime;
        output_initial_lifetimes[spawn_index] = lifetime;
        output_spawn_times[spawn_index] = simulation_time;
    }

    number_spawned += n_to_add;
}

/* **************************************** */
// Movement and collision
/* **************************************** */
void Sim::expire_instances(float const dt, ml::FrameScratch& scratch) {
    auto const entities{this->entities.get_view()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const active{entities.active()};

    ml::subtract_in_place(std::span<float>{lifetimes}, dt);

    ml::FrameArray<std::int32_t> expired_indices{&scratch};
    auto const count{entities.num()};
    expired_indices.reserve(count);
    for (std::int32_t index{count - 1}; index >= 0; --index) {
        if (active[index] != 0 && lifetimes[index] <= 0.f) {
            active[index] = 0;
            expired_indices.add(index);
        }
    }
    pending_removals_.insert(
        pending_removals_.end(), expired_indices.begin(), expired_indices.end());
}
void Sim::update_locations(float const dt) {
    auto const entities{this->entities.get_view()};
    auto const count{entities.num()};
    auto const active{entities.active()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const locations{entities.view_locations()};
    auto const velocities{entities.view_velocities()};

    for (std::int32_t index{}; index < count; ++index) {
        if (active[index] != 0) {
            auto const step{std::min(dt, std::max(0.f, lifetimes[index]))};
            set_vector(locations,
                       index,
                       vector_at(locations, index) + vector_at(velocities, index) * step);
        }
    }
}
void Sim::handle_collisions(float const dt, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::handle_collisions");

    auto const n{entities.num()};
    if (n < 1) {
        return;
    }

    FrameCollisionScratch collision_scratch{scratch};
    collision_scratch.set_num(n);
    auto const entities{this->entities.get_const_view()};
    auto const locations{entities.view_locations().get_const_view()};
    auto const velocities{entities.view_velocities().get_const_view()};
    assert(config.collision_jobs > 0);
    auto const job_count{std::min(n, config.collision_jobs)};
    auto const updates_per_slice{n / job_count + (n % job_count != 0)};
    ml::FrameArray<std::int32_t> jobs{&scratch};
    jobs.set_num(job_count);
    auto const job_indices{jobs.view()};
    std::iota(job_indices.begin(), job_indices.end(), 0);
    auto const active_rows{entities.active()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const instigator_ids{entities.instigator_ids()};

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
                auto const start{vector_at(trace_locations, trace_index)};
                trace_starts.set(trace_index, start);
                auto const index{i_start + trace_index};
                auto const step{
                    active_rows[index] != 0 ? std::min(dt, std::max(0.f, lifetimes[index])) : 0.f};
                trace_ends.set(trace_index,
                               start + vector_at(trace_velocities, trace_index) * step);
            }

            auto const trace_starts_view{
                collision_scratch.trace_starts.get_const_view().slice(i_start, trace_count)};
            auto const trace_ends_view{
                collision_scratch.trace_ends.get_const_view().slice(i_start, trace_count)};
            auto const hits{collision_scratch.trace_hits.get_view().slice(i_start, trace_count)};
            auto const ignored_entities{
                std::span<EntityUniqueId const>{instigator_ids}.subspan(i_start, trace_count)};
            query_manager.trace_closest_lines(
                trace_starts_view, trace_ends_view, hits, ignored_entities);
        });

    ml::FrameArray<std::int32_t> to_remove{&scratch};
    FrameHitDetails hit_details{scratch};
    FrameDirectDamageEvents collision_damage_events{scratch};
    auto const trace_hits{collision_scratch.trace_hits.get_const_view()};
    auto const hit_count{static_cast<std::int32_t>(
        std::ranges::count_if(trace_hits.hits, [](auto const hit) { return hit != 0; }))};
    to_remove.reserve(hit_count);
    collision_damage_events.reserve(hit_count);
    hit_details.reserve(hit_count);
    auto const damages{entities.damages()};
    auto const sources{entities.sources()};

    constexpr float safe_normal_tolerance{1.e-8f};
    for (std::int32_t entity_index{}; entity_index < n; ++entity_index) {
        auto const element{static_cast<std::size_t>(entity_index)};
        if (active_rows[element] == 0 || trace_hits.hits[element] == 0) {
            continue;
        }

        to_remove.add(entity_index);
        auto const damaged_entity{trace_hits.entities[element]};
        if (damaged_entity.is_valid()) {
            auto const instigator{instigator_ids[element]};
            collision_damage_events.add(damaged_entity, damages[element], instigator);
        }

        auto const velocity{vector_at(velocities, entity_index)};
        auto const length_squared{HMM_LenSqrV3(velocity)};
        auto const emission_direction{length_squared < safe_normal_tolerance
                                          ? HMM_V3(0.f, 0.f, -1.f)
                                          : velocity * (-1.f / std::sqrt(length_squared))};
        hit_details.add(trace_hits.locations[entity_index], emission_direction, sources[element]);
    }
    std::ranges::sort(to_remove.view(), std::greater{});
    combat_events.queue_damage(collision_damage_events.get_const_view());

    auto const active{this->entities.get_view().active()};
    for (auto const index : to_remove) {
        active[index] = 0;
        pending_removals_.push_back(index);
    }

    frame_output_.append_hits(hit_details, simulation_clock.get_completed_ticks());
}
void Sim::remove_instances(std::span<std::int32_t const> const indices) {
    assert(std::ranges::is_sorted(indices, std::greater{}));
    for (auto const index : indices) {
        entities.remove_at_swap(index, 1);
    }
}

/* **************************************** */
// Buffer cleanup and validation
/* **************************************** */
void Sim::clear_spawn_buffers() {
    pending_spawns.reset();
}

} // namespace lasers

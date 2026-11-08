#include "ioj/sim/lasers/sim.h"

#include <ioj/column_math.h>
#include <ioj/generated/array_math_kernels.h>
#include <ioj/parallel_for.h>
#include <ioj/sim/collision/spatial_query_manager.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/lasers/laser_hit_details.h>
#include <ioj/sim/phase_checks.h>
#include <ioj/sim/profiling.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/vectors3f.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <span>
#include <vector>

namespace ioj::sim::lasers {

/* **************************************** */
// Construction and tick phases
/* **************************************** */
// Level initialisation
void Sim::initialise(LaserSimConfig const& new_config) {
    config_ = new_config;

    SANDBOX_PROFILE_SCOPE("lasers::Sim::initialise");
    profiling::plot("Sandbox/LaserCount", 0);

    number_spawned_ = 0;
    preallocate_instances();
}
auto Sim::append_spawn_requests(std::uint32_t const count) -> LaserSpawnRequests::View {
    pending_spawns_.add_uninitialised(count);
    return pending_spawns_.right(count);
}

// Preparation
void Sim::commit_spawns(PreparationInputs const inputs) {
    assert(is_preparation_mutation_allowed(inputs.clock));
    SANDBOX_PROFILE_SCOPE("lasers::Sim::commit_spawns");
    process_pending_spawns(inputs);
    clear_spawn_buffers();
}

// Action
void Sim::simulate(ActionInputs const inputs) {
    auto const dt{inputs.dt};

    SANDBOX_PROFILE_SCOPE("lasers::Sim::simulate");

    handle_collisions(inputs);
    update_locations(dt);
    expire_instances(dt, inputs.pending_removals);
}

// ResolutionCommit
void Sim::cleanup_entities(ResolutionCommitInputs const inputs) {
    auto& pending_removals{inputs.pending_removals};
    assert(inputs.clock.phase() == SimulationPhase::ResolutionCommit);
    std::ranges::sort(pending_removals, std::greater{});
    remove_instances(pending_removals);
}
void Sim::finish_action() {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::finish_action");
    profiling::plot("Sandbox/LaserCount", num_instances());
}

auto Sim::num_instances() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(
        std::ranges::count(entities_.const_view().active(), std::uint8_t{1}));
}

/* **************************************** */
// Spawning
/* **************************************** */

void Sim::preallocate_instances() {
    entities_.reserve(config_.n_preallocated_instances);
}

void Sim::process_pending_spawns(PreparationInputs const inputs) {
    SANDBOX_PROFILE_SCOPE("lasers::Sim::process_pending_spawns");

    auto const n_to_add{pending_spawns_.num()};
    inputs.combat_events.record_shots(pending_spawns_.const_view().instigator_ids());

    if (n_to_add <= 0) {
        return;
    }

    auto const requests{pending_spawns_.const_view()};
    auto const simulation_time{static_cast<float>(inputs.clock.simulation_time())};
    constexpr float fixed_spawn_offset{10.f};
    entities_.add_uninitialised(n_to_add);
    auto const output{entities_.view().right(n_to_add)};
    auto const output_active{output.active()};
    auto const output_locations{output.view_locations().resolve()};
    auto const output_rotations{output.view_rotations().resolve()};
    auto const output_velocities{output.view_velocities().resolve()};
    auto const output_sources{output.sources()};
    auto const output_damages{output.damages()};
    auto const output_instigator_ids{output.instigator_ids()};
    auto const output_lifetimes_remaining{output.lifetimes_remaining()};
    auto const output_initial_lifetimes{output.initial_lifetimes()};
    auto const output_spawn_times{output.spawn_times()};

    auto const requests_speeds{requests.speeds()};
    auto const requests_max_distances{requests.max_distances()};
    auto const requests_rotations{requests.view_rotations().resolve()};
    auto const requests_locations{requests.view_locations().resolve()};
    auto const requests_base_velocities{requests.view_base_velocities().resolve()};
    auto const requests_sources{requests.sources()};
    auto const requests_damages{requests.damages()};
    auto const requests_instigator_ids{requests.instigator_ids()};

    for (std::uint32_t spawn_index{}; spawn_index < n_to_add; ++spawn_index) {
        output_active[spawn_index] = 1;
        auto const speed{requests_speeds[spawn_index]};
        auto const lifetime{requests_max_distances[spawn_index] / speed};
        auto const rotation{rotation_at(requests_rotations, spawn_index)};
        auto const pitch{rotation.pitch * (std::numbers::pi_v<float> / 180.f)};
        auto const yaw{rotation.yaw * (std::numbers::pi_v<float> / 180.f)};
        auto const cos_pitch{std::cos(pitch)};
        auto const direction{
            Vector3f{cos_pitch * std::cos(yaw), cos_pitch * std::sin(yaw), std::sin(pitch)}};
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

    number_spawned_ += n_to_add;
}

/* **************************************** */
// Movement and collision
/* **************************************** */
void Sim::expire_instances(float const dt, std::pmr::vector<LaserIndex>& pending_removals) {
    auto const entities{entities_.view()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const active{entities.active()};

    subtract_in_place(std::span<float>{lifetimes}, dt);

    auto const count{entities.num()};
    for (auto remaining{count}; remaining > 0; --remaining) {
        auto const index{remaining - 1};
        if (active[index] != 0 && lifetimes[index] <= 0.f) {
            active[index] = 0;
            pending_removals.push_back(index);
        }
    }
}
void Sim::update_locations(float const dt) {
    auto const entities{entities_.view()};
    auto const count{entities.num()};
    auto const active{entities.active()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const locations{entities.view_locations().resolve()};
    auto const velocities{entities.view_velocities().resolve()};

    for (std::uint32_t index{}; index < count; ++index) {
        if (active[index] != 0) {
            auto const step{std::min(dt, std::max(0.f, lifetimes[index]))};
            set_vector(locations,
                       index,
                       vector_at(locations, index) + vector_at(velocities, index) * step);
        }
    }
}
void Sim::handle_collisions(ActionInputs const inputs) {
    auto* const scratch_resource{&inputs.scratch_resource};
    auto const dt{inputs.dt};

    SANDBOX_PROFILE_SCOPE("lasers::Sim::handle_collisions");

    auto const n{entities_.num()};
    if (n < 1) {
        return;
    }

    Vectors3f end_storage{*scratch_resource};
    TraceHits hit_storage{*scratch_resource};
    end_storage.set_num(n);
    hit_storage.set_num(n);
    auto const trace_ends{end_storage.view().resolve()};
    auto const trace_results{hit_storage.view()};
    auto const entities{entities_.const_view()};
    auto const locations{entities.view_locations().const_view().resolve()};
    auto const velocities{entities.view_velocities().const_view().resolve()};
    auto const active_rows{entities.active()};
    auto const lifetimes{entities.lifetimes_remaining()};
    auto const instigator_ids{entities.instigator_ids()};

    parallel_for(n, [=](std::uint32_t const begin, std::uint32_t const end) {
        SANDBOX_PROFILE_SCOPE("trace_job");

        auto const trace_count{end - begin};
        auto const trace_locations{locations.slice(begin, trace_count)};
        auto const trace_velocities{velocities.slice(begin, trace_count)};
        auto const ends{trace_ends.slice(begin, trace_count)};
        for (std::uint32_t trace_index{}; trace_index < trace_count; ++trace_index) {
            auto const start{vector_at(trace_locations, trace_index)};
            auto const index{begin + trace_index};
            auto const step{active_rows[index] != 0 ? std::min(dt, std::max(0.f, lifetimes[index]))
                                                    : 0.f};
            ends.set(trace_index, start + vector_at(trace_velocities, trace_index) * step);
        }

        auto const hits{trace_results.slice(begin, trace_count)};
        auto const ignored_entities{
            std::span<EntityId const>{instigator_ids}.subspan(begin, trace_count)};
        inputs.spatial_query_manager.trace_closest_lines(
            trace_locations, ends.const_view(), hits, ignored_entities);
    });

    LaserHitDetails hit_details{*scratch_resource};
    DirectDamageEvents collision_damage_events{*scratch_resource};
    auto const trace_hits{hit_storage.const_view()};
    auto const hit_flags{trace_hits.hits()};
    auto const hit_entities{trace_hits.entities()};
    auto const hit_locations{trace_hits.view_locations().resolve()};
    auto const hit_count{static_cast<std::uint32_t>(
        std::ranges::count_if(hit_flags, [](auto const hit) { return hit.value() != 0; }))};
    inputs.pending_removals.reserve(inputs.pending_removals.size() + hit_count);
    collision_damage_events.reserve(hit_count);
    hit_details.reserve(hit_count);
    auto const damages{entities.damages()};
    auto const sources{entities.sources()};

    auto const active{entities_.view().active()};

    constexpr float safe_normal_tolerance{1.e-8f};
    for (std::uint32_t entity_index{}; entity_index < n; ++entity_index) {
        auto const element{static_cast<std::size_t>(entity_index)};
        if (active_rows[element] == 0 || hit_flags[element].value() == 0) {
            continue;
        }

        active[element] = 0;
        inputs.pending_removals.push_back(entity_index);
        auto const damaged_entity{hit_entities[element]};
        if (damaged_entity.is_valid()) {
            auto const instigator{instigator_ids[element]};
            collision_damage_events.add(damaged_entity, damages[element], instigator);
        }

        auto const velocity{vector_at(velocities, entity_index)};
        auto const length_squared{size_squared(velocity)};
        auto const emission_direction{length_squared < safe_normal_tolerance
                                          ? Vector3f{0.f, 0.f, -1.f}
                                          : velocity * (-1.f / std::sqrt(length_squared))};
        auto const hit_location{hit_locations[entity_index]};
        hit_details.add(hit_location.x(),
                        hit_location.y(),
                        hit_location.z(),
                        emission_direction.x(),
                        emission_direction.y(),
                        emission_direction.z(),
                        sources[element]);
    }
    inputs.combat_events.queue_damage(collision_damage_events.const_view());

    frame_output_.append_hits(hit_details.const_view(), inputs.tick);
}
void Sim::remove_instances(std::span<LaserIndex const> const indices) {
    for (auto const index : indices) {
        entities_.remove_at_swap(index, 1);
    }
}

/* **************************************** */
// Buffer cleanup and validation
/* **************************************** */
void Sim::clear_spawn_buffers() {
    pending_spawns_.reset();
}

} // namespace lasers

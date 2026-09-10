#include "SpaceGameSimulation/defences/turrets/TestStaticTurretsSimulation.h"

#include <SpaceGameSimulation/combat/lasers/TestLasersFrameScratch.h>
#include <SpaceGameSimulation/entities/BatchSimulation.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/fixed_array.h>
#include <SandboxCore/frame_array.h>
#include <SandboxCore/frame_memory_resource.h>
#include <SandboxCore/frame_rotators.h>
#include <SandboxCore/frame_vectors.h>
#include <SandboxCore/loop_bounds.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxNative/deterministic_bias.h>

#include <Async/ParallelFor.h>
#include <HAL/PlatformMisc.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestStaticTurretCount, TEXT("Sandbox/TestStaticTurretCount"));

namespace ml::test_static_turrets {
namespace scratch {
template <typename T>
void reserve(TArray<T>& values, int32 const count) {
    values.Reserve(count);
}
template <typename T>
void reserve(TFrameArray<T>& values, int32 const count) {
    values.reserve(count);
}
template <typename T>
void add(TArray<T>& values, T const& value) {
    values.Add(value);
}
template <typename T>
void add(TFrameArray<T>& values, T const& value) {
    values.add(value);
}
template <typename T>
auto num(TArray<T> const& values) -> int32 {
    return values.Num();
}
template <typename T>
auto num(TFrameArray<T> const& values) -> int32 {
    return values.num();
}
template <typename T>
void set_num_uninitialized(TArray<T>& values, int32 const count) {
    values.SetNumUninitialized(count, EAllowShrinking::No);
}
template <typename T>
void set_num_uninitialized(TFrameArray<T>& values, int32 const count) {
    values.reserve(count);
    for (int32 i{}; i < count; ++i) {
        values.emplace();
    }
}

#if WITH_DEV_AUTOMATION_TESTS
auto allocated_size(FVectors3f const& values) -> SIZE_T {
    return values.xs.GetAllocatedSize() + values.ys.GetAllocatedSize() +
           values.zs.GetAllocatedSize();
}
auto allocated_size(ml::test_lasers::SpawnRequests const& values) -> SIZE_T {
    return allocated_size(values.locations) + values.rotations.pitches.GetAllocatedSize() +
           values.rotations.yaws.GetAllocatedSize() + values.rotations.rolls.GetAllocatedSize() +
           allocated_size(values.base_velocities) + values.damages.GetAllocatedSize() +
           values.speeds.GetAllocatedSize() + values.max_distances.GetAllocatedSize() +
           values.instigator_handles.GetAllocatedSize() + values.sources.GetAllocatedSize();
}
#endif
}

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
                                  FRotatorsf::ConstView const rotations)
    -> TArray<FRegistryEntityHandle> {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::register_turrets);
    spawn_data.validate_array_sizes();
    auto const n_to_add{spawn_data.num()};
    if (n_to_add == 0) {
        return {};
    }

    check(config.target_refresh_frequency > 0.f);
    auto const target_refresh_tick_period_unsigned{
        simulation_clock.frequency_to_tick_period(config.target_refresh_frequency)};
    check(FPeriodicTickCountdown16::valid_period(target_refresh_tick_period_unsigned));
    auto const target_refresh_tick_period{
        static_cast<FPeriodicTickCountdown16::counter_type>(target_refresh_tick_period_unsigned)};

    auto const first_new_index{entities.num()};
    entities.add_uninitialised(n_to_add);
    ml::assign_from(entities.locations.get_view(first_new_index, n_to_add), spawn_data.locations);
    for (int32 local_index{}; local_index < n_to_add; ++local_index) {
        auto const i{first_new_index + local_index};
        entities.teams[i] = spawn_data.teams[local_index];
        entities.healths[i] = spawn_data.healths[local_index];
        entities.laser_damages[i] = spawn_data.laser_damages[local_index];
    }
    entities.target_refresh_countdowns.initialise_last(target_refresh_tick_period, n_to_add);
    ml::fill(
        TArrayView<FRegistryEntityHandle>{entities.target_handles}.Slice(first_new_index, n_to_add),
        FRegistryEntityHandle{});
    ml::fill(entities.target_locations.get_view(first_new_index, n_to_add), 0.f);
    ml::fill(entities.target_velocities.get_view(first_new_index, n_to_add), 0.f);
    entities.laser_cooldowns.zero_last(n_to_add);

    FVector3f const fire_point_offset{config.fire_point_offset.GetLocation()};
    for (int32 local_index{}; local_index < n_to_add; ++local_index) {
        auto const i{first_new_index + local_index};
        entities.fire_point_locations.set(i,
                                          entities.locations.xs[i] + fire_point_offset.X,
                                          entities.locations.ys[i] + fire_point_offset.Y,
                                          entities.locations.zs[i] + fire_point_offset.Z);

        entities.target_refresh_countdowns.remaining_ticks[i] =
            static_cast<FPeriodicTickCountdown16::counter_type>(target_refresh_next_offset);
        ++target_refresh_next_offset;
        if (target_refresh_next_offset == target_refresh_tick_period) {
            target_refresh_next_offset = 0;
        }
    }

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    ml::assign_from(new_entity_data.locations, spawn_data.locations);
    for (int32 i{}; i < n_to_add; ++i) {
        auto const rotation{ml::get_rotator3d(rotations, i)};
        ml::assign(new_entity_data.rotations, i, rotation);
        ml::assign(entities.rotations, first_new_index + i, rotation);
    }
    ml::fill(new_entity_data.velocities, 0.f);
    ml::fill(new_entity_data.radii, entity_radius);
    new_entity_data.set_all_entity_types(ETestEntityType::Turret);
    for (int32 i{}; i < n_to_add; ++i) {
        new_entity_data.healths[i] = spawn_data.healths[i];
        new_entity_data.teams[i] = spawn_data.teams[i];
        new_entity_data.alive[i] = static_cast<uint8>(spawn_data.healths[i] > 0);
    }
    auto const new_entities{entity_registry.add_entities(new_entity_data.get_const_view())};
    auto new_handles{new_entities.registry_handles.to_array()};
    for (int32 local_index{}; local_index < n_to_add; ++local_index) {
        entities.handles[first_new_index + local_index] = new_handles[local_index];
    }
    ml::make_deterministic_biases(
        TConstArrayView<FRegistryEntityHandle>{entities.handles}.Slice(first_new_index, n_to_add),
        TArrayView<uint32>{entities.integral_biases}.Slice(first_new_index, n_to_add));
    validate_array_sizes();
    for (int32 i{}; i < n_to_add; ++i) {
        auto const index{first_new_index + i};
        frame_changes_.Add({.kind = EEntityFrameChange::Spawn,
                            .index = index,
                            .transform = FTransform{ml::get_rotator3d(entities.rotations, index),
                                                    ml::get_vector3d(entities.locations, index)},
                            .team = entities.teams[index],
                            .handle = entities.handles[index]});
    }
    return new_handles;
}

/* **************************************** */
// Death handling
/* **************************************** */
void Simulation::handle_dead_entities() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::handle_dead_entities);
    if (local_indices_to_remove.IsEmpty()) {
        return;
    }

    ml::batch::sort_and_deduplicate_removal_indices(local_indices_to_remove);

    death_locations_.Reserve(local_indices_to_remove.Num());
    for (auto const index : local_indices_to_remove) {
        death_locations_.Add(ml::get_vector3f(entities.locations, index));
        frame_changes_.Add({.kind = EEntityFrameChange::RemoveSwap,
                            .index = index,
                            .handle = entities.handles[index]});
    }
    ml::remove_at_swap_many_sorted_desc(local_indices_to_remove, entities);
}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::begin_play);
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, 0);
    check(entity_radius > 0.f);
    check(search_slice_size > 0);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    entities.laser_cooldowns.set_tick_value(cooldown_tick_period);
    validate_array_sizes();
}
void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::begin_tick);
    clear_tick_buffers();
}
void Simulation::update_timers(float const) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::update_timers);

    entities.laser_cooldowns.tick();
    entities.target_refresh_countdowns.tick();
}
void Simulation::make_decisions() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::make_decisions);
    perform_search();
}
void Simulation::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::queue_commands);

    fire_at_enemies();
}
void Simulation::resolve_damage_events() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::resolve_damage_events);

    ml::batch::resolve_damage_events(entity_registry,
                                     entities.handles,
                                     entities.healths,
                                     local_indices_to_remove,
                                     entity_death_info);
    validate_array_sizes();
}
void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::update_entity_registry);

    prepare_entity_update_data();

    entity_registry.queue_entity_updates(
        {
            .indices = entities.handles,
            .data = entity_update_data.get_const_view(),
        },
        entity_death_info);
}
void Simulation::sync_from_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::sync_from_registry);

    entity_registry.refresh_entity_data(entities.target_handles,
                                        entities.target_locations.get_view(),
                                        entities.target_velocities.get_view(),
                                        {});

    handle_dead_entities();
}
void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::end_tick);
    TRACE_COUNTER_SET(SandboxTestStaticTurretCount, get_num_instances());

    validate_array_sizes();
}

/* **************************************** */
// Entity data
/* **************************************** */
void Simulation::prepare_entity_update_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        Sandbox::test_static_turrets::Simulation::prepare_entity_update_data);
    check(entity_update_data.num() == 0);

    auto const n{get_num_instances()};

    ml::add_uninitialised(entity_update_data, n);

    entity_update_data.locations = entities.locations;
    entity_update_data.rotations = entities.rotations;
    ml::fill(entity_update_data.velocities, 0.f);
    ml::fill(entity_update_data.radii, entity_radius);
    entity_update_data.healths = entities.healths;
    entity_update_data.teams = entities.teams;
    entity_update_data.set_all_entity_types(ETestEntityType::Turret);

    for (int32 i{0}; i < n; ++i) {
        entity_update_data.alive[i] = static_cast<uint8>(entities.healths[i] > 0);
    }
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Simulation::get_num_instances() const noexcept -> int32 {
    return entities.handles.Num();
}
auto Simulation::get_target_handles() const -> TConstArrayView<FRegistryEntityHandle> {
    return entities.target_handles;
}
#if WITH_DEV_AUTOMATION_TESTS
auto Simulation::get_persistent_scratch_allocated_bytes() const noexcept -> SIZE_T {
    return scratch_int_buffer_.GetAllocatedSize() +
           line_of_sight_hit_entity_handles_.GetAllocatedSize() +
           scratch::allocated_size(line_of_sight_start_locations_) +
           scratch::allocated_size(line_of_sight_end_locations_) +
           scratch::allocated_size(new_lasers_);
}
#endif

/* **************************************** */
// Searching
/* **************************************** */
void Simulation::perform_search() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::perform_search);

    auto const n_turrets{get_num_instances()};
    if (n_turrets == 0) {
        return;
    }

    auto const radius{config.detection_radius};

    auto const hardware_thread_count{
        FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads())};
    auto const max_jobs_for_grain_size{FMath::Max(1, n_turrets / search_slice_size)};
    auto const n_jobs{FMath::Min(hardware_thread_count, max_jobs_for_grain_size)};
    auto const turrets_per_job{FMath::DivideAndRoundUp(n_turrets, n_jobs)};

    ParallelFor(n_jobs, [=, this](int32 const i) {
        perform_search_on_slice(i, n_turrets, turrets_per_job, radius);
    });
}
void Simulation::perform_search_on_slice(int32 const job_index,
                                         int32 const n_turrets,
                                         int32 const turrets_per_job,
                                         float const radius) {
    auto const begin{job_index * turrets_per_job};
    auto const end{FMath::Min(begin + turrets_per_job, n_turrets)};
    TFixedVectors3f<128> candidate_locations;
    ml::TFixedArray<uint8, 128> has_line_of_sight;

    for (int32 i{begin}; i < end; ++i) {
        if (!entities.target_refresh_countdowns.try_consume(i)) {
            continue;
        }

        if (entities.target_handles[i].is_null()) {
            auto const turret_location{ml::get_vector3f(entities.locations, i)};
            auto const this_team{entities.teams[i]};

            ml::TFixedArray<FRegistryEntityHandle, 128> target_handles;
            target_handles.set_num_uninitialised(
                spatial_query_manager.collect_non_team_entities_in_range(
                    turret_location, this_team, radius, target_handles.capacity_view()));

            entities.target_handles[i] = FRegistryEntityHandle{};

            auto const target_count{target_handles.num()};
            candidate_locations.set_num_uninitialised(target_count);
            has_line_of_sight.set_num_uninitialised(target_count);
            auto candidate_locations_view{candidate_locations.get_view()};
            for (int32 target_index{}; target_index < target_count; ++target_index) {
                candidate_locations_view.set(
                    target_index, entity_registry.get_location(target_handles[target_index]));
            }

            spatial_query_manager.has_line_of_sight_to_targets(
                ml::get_vector3f(entities.fire_point_locations, i),
                candidate_locations.get_const_view(),
                target_handles,
                has_line_of_sight);

            auto const target_offset{target_count == 0
                                         ? 0
                                         : static_cast<int32>(entities.integral_biases[i] %
                                                              static_cast<uint32>(target_count))};
            auto const loop_bounds{ml::make_rotated_loop_bounds(0, target_count, target_offset)};
            for (auto const bounds : loop_bounds) {
                for (int32 target_index{bounds.begin}; target_index < bounds.end; ++target_index) {
                    auto const target_handle{target_handles[target_index]};
                    if (has_line_of_sight[target_index] == 0) {
                        continue;
                    }
                    if (this_team == entity_registry.get_team(target_handle)) {
                        continue;
                    }

                    entities.target_handles[i] = target_handle;
                    goto target_found;
                }
            }
        }

    target_found:;
    }
}

/* **************************************** */
// Attacking
/* **************************************** */
void Simulation::fire_at_enemies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::fire_at_enemies);

#if WITH_DEV_AUTOMATION_TESTS
    if (scratch_allocation_mode_ == EScratchAllocationMode::Persistent) {
        scratch_int_buffer_.Reset();
        line_of_sight_hit_entity_handles_.Reset();
        line_of_sight_start_locations_.reset();
        line_of_sight_end_locations_.reset();
        new_lasers_.reset();
        fire_at_enemies_with_scratch(scratch_int_buffer_,
                                     line_of_sight_hit_entity_handles_,
                                     line_of_sight_start_locations_,
                                     line_of_sight_end_locations_,
                                     new_lasers_);
        return;
    }
#endif

    TFrameArray<int32> candidate_indices{&frame_memory_resource};
    TFrameArray<FRegistryEntityHandle> hit_entity_handles{&frame_memory_resource};
    FFrameVectors3f start_locations{&frame_memory_resource};
    FFrameVectors3f end_locations{&frame_memory_resource};
    ml::test_lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    fire_at_enemies_with_scratch(
        candidate_indices, hit_entity_handles, start_locations, end_locations, new_lasers);
}

template <typename CandidateIndices,
          typename HitEntityHandles,
          typename StartLocations,
          typename EndLocations,
          typename LaserSpawns>
void Simulation::fire_at_enemies_with_scratch(CandidateIndices& candidate_indices,
                                              HitEntityHandles& hit_entity_handles,
                                              StartLocations& start_locations,
                                              EndLocations& end_locations,
                                              LaserSpawns& new_lasers) {
    auto const n{get_num_instances()};
    auto const laser_speed{config.laser.projectile_speed};
    auto const laser_max_distance{config.laser.max_distance};

    auto const disengage_radius{get_disengage_radius()};
    auto const disengage_radius_sq{disengage_radius * disengage_radius};

    scratch::reserve(candidate_indices, n);
    start_locations.reserve(n);
    end_locations.reserve(n);

    for (int32 i{0}; i < n; ++i) {
        auto const target_handle{entities.target_handles[i]};

        if (target_handle.is_null()) {
            continue;
        }

        if (!entity_registry.is_valid_alive(target_handle)) {
            entities.target_handles[i].reset();
            continue;
        }

        if (!entities.laser_cooldowns.is_ready(i)) {
            continue;
        }

        auto const turret_location{ml::get_vector3f(entities.locations, i)};
        auto const target_location{ml::get_vector3f(entities.target_locations, i)};

        auto const distance_sq{FVector3f::DistSquared(turret_location, target_location)};
        if (distance_sq >= disengage_radius_sq) {
            entities.target_handles[i].reset();
            continue;
        }

        scratch::add(candidate_indices, i);
        start_locations.add(entities.fire_point_locations.xs[i],
                            entities.fire_point_locations.ys[i],
                            entities.fire_point_locations.zs[i]);
        end_locations.add(target_location);

        entities.laser_cooldowns.restart_counter(i);
    }

    auto const n_candidates{scratch::num(candidate_indices)};
    if (n_candidates == 0) {
        return;
    }

    scratch::set_num_uninitialized(hit_entity_handles, n_candidates);
    new_lasers.reserve(n_candidates);
    spatial_query_manager.trace_line_of_sight(
        start_locations.get_const_view(), end_locations.get_const_view(), hit_entity_handles);

    for (int32 candidate_index{0}; candidate_index < n_candidates; ++candidate_index) {
        auto const i{candidate_indices[candidate_index]};
        if (hit_entity_handles[candidate_index] != entities.target_handles[i]) {
            continue;
        }

        auto const target_location{ml::get_vector3f(entities.target_locations, i)};
        auto const loc_x{entities.fire_point_locations.xs[i]};
        auto const loc_y{entities.fire_point_locations.ys[i]};
        auto const loc_z{entities.fire_point_locations.zs[i]};
        FVector3f const laser_location{
            loc_x,
            loc_y,
            loc_z,
        };

        auto const target_velocity{ml::get_vector3f(entities.target_velocities, i)};
        auto const intercept_time{ml::solve_intercept_time(
            laser_location, target_location, target_velocity, laser_speed)};

        FVector3f const intercept_pos{target_location + target_velocity * intercept_time};
        FVector3f const fire_dir{(intercept_pos - laser_location).GetSafeNormal()};

        new_lasers.add(laser_location,
                       fire_dir.ToOrientationRotator(),
                       FVector3f::ZeroVector,
                       entities.laser_damages[i],
                       laser_speed,
                       laser_max_distance,
                       entities.handles[i],
                       FLaserSource{entities.teams[i], ETestEntityType::Turret});
    }

    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}
auto Simulation::get_disengage_radius() const -> float {
    return config.detection_radius * 1.2f;
}

/* **************************************** */
// Misc
/* **************************************** */
void Simulation::clear_tick_buffers() {
    ml::reset(entity_death_info, entity_update_data, local_indices_to_remove);
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

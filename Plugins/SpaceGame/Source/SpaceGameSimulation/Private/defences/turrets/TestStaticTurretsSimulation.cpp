#include "SpaceGameSimulation/defences/turrets/TestStaticTurretsSimulation.h"

#include <sandbox/simulation/turret_firing.h>
#include <sandbox/simulation/turret_spawn_initialization.h>
#include <sandbox/simulation/turret_targeting.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersFrameScratch.h>
#include <SpaceGameSimulation/entities/BatchSimulation.h>
#include <SpaceGameSimulation/entities/NativeEntityRegistryView.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
#include <SpaceGameSimulation/simulation/SpatialQueryManager.h>

#include <SandboxCore/array_utils.h>
#include <SandboxCore/fixed_array.h>
#include <SandboxCore/frame_memory_resource.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxNative/deterministic_bias.h>

#include <Async/ParallelFor.h>
#include <HAL/PlatformMisc.h>
#include <ProfilingDebugging/CountersTrace.h>

TRACE_DECLARE_INT_COUNTER(SandboxTestStaticTurretCount, TEXT("Sandbox/TestStaticTurretCount"));

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
    entities.add_defaulted(n_to_add);
    auto const new_entities_view{entities.get_view(first_new_index, n_to_add)};
    auto const spawn_count{static_cast<std::size_t>(n_to_add)};
    ml::simulation::turrets::initialize_spawned_turrets(
        {.locations = ml::to_native(new_entities_view.locations),
         .fire_point_locations = ml::to_native(new_entities_view.fire_point_locations),
         .teams = std::as_writable_bytes(std::span{new_entities_view.teams.GetData(), spawn_count}),
         .healths = {new_entities_view.healths.GetData(), spawn_count},
         .laser_damages = {new_entities_view.laser_damages.GetData(), spawn_count},
         .refresh_remaining_ticks = {entities.target_refresh_countdowns.remaining_ticks.GetData() +
                                         first_new_index,
                                     spawn_count},
         .refresh_periods = {entities.target_refresh_countdowns.periods.GetData() + first_new_index,
                             spawn_count}},
        spawn_data,
        ml::to_native(FVector3f{config.fire_point_offset.GetLocation()}),
        target_refresh_tick_period,
        target_refresh_next_offset);

    RegistryEntityData new_entity_data;
    new_entity_data.add_uninitialised(n_to_add);
    ml::assign_from(new_entity_data.locations, ml::to_unreal(spawn_data.locations));
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
        new_entity_data.teams[i] = ml::to_unreal(spawn_data.teams[i]);
        new_entity_data.alive[i] = static_cast<uint8>(spawn_data.healths[i] > 0);
    }
    auto const new_entities{entity_registry.add_entities(new_entity_data.get_const_view())};
    auto new_handles{ml::to_registry_entity_handle_array(new_entities.registry_handles)};
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
    entity_update_data.reset();

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
    auto const registry_view{ml::make_native_query_view(entity_registry)};
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

            entities.target_handles[i] = ml::simulation::select_turret_target(
                {target_handles.data(), static_cast<std::size_t>(target_count)},
                {has_line_of_sight.data(), static_cast<std::size_t>(target_count)},
                registry_view.teams,
                ml::to_native(this_team),
                entities.integral_biases[i]);
        }
    }
}

/* **************************************** */
// Attacking
/* **************************************** */
void Simulation::fire_at_enemies() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_static_turrets::Simulation::fire_at_enemies);

    auto const count{static_cast<std::size_t>(get_num_instances())};
    ml::simulation::turrets::FiringView const firing_view{
        .locations = ml::to_native(entities.locations.get_const_view()),
        .fire_point_locations = ml::to_native(entities.fire_point_locations.get_const_view()),
        .target_locations = ml::to_native(entities.target_locations.get_const_view()),
        .target_velocities = ml::to_native(entities.target_velocities.get_const_view()),
        .handles = {entities.handles.GetData(), count},
        .targets = {entities.target_handles.GetData(), count},
        .laser_damages = {entities.laser_damages.GetData(), count},
        .teams = std::as_bytes(std::span{entities.teams.GetData(), count}),
        .cooldowns = entities.laser_cooldowns.get_view().native_view()};
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

    spatial_query_manager.trace_line_of_sight(ml::to_unreal(scratch.starts.get_const_view()),
                                              ml::to_unreal(scratch.ends.get_const_view()),
                                              {scratch.hit_handles.data(), candidate_count});

    ml::simulation::lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    ml::simulation::turrets::emit_lasers(firing_view,
                                         scratch,
                                         config.laser.projectile_speed,
                                         config.laser.max_distance,
                                         UE_SMALL_NUMBER,
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

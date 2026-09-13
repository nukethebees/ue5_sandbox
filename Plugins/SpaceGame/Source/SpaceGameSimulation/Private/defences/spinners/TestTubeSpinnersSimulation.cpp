#include "SpaceGameSimulation/defences/spinners/TestTubeSpinnersSimulation.h"
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include <SpaceGameSimulation/combat/lasers/TestLasersFrameScratch.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml::test_tube_spinners {

/* **************************************** */
// Configuration
/* **************************************** */
void Simulation::set_config(FSpinnerSimulationConfig const& new_config) noexcept {
    config = new_config;
    fire_points_.clear();
    fire_points_.reserve(static_cast<std::size_t>(config.fire_point_offsets.Num()));
    for (auto const& offset : config.fire_point_offsets) {
        fire_points_.push_back({ml::to_native(FVector3f{offset.GetLocation()}),
                                ml::to_native(FRotator3f{offset.Rotator()})});
    }
}
Simulation::Simulation(FSimulationClock const& clock,
                       FTestEntityRegistry& in_entity_registry,
                       ml::test_lasers::Simulation& in_laser_simulation,
                       std::pmr::memory_resource& in_frame_memory_resource) noexcept
    : simulation_clock{clock}
    , entity_registry{in_entity_registry}
    , laser_simulation{in_laser_simulation}
    , frame_memory_resource{in_frame_memory_resource} {}

/* **************************************** */
// Simulation phases
/* **************************************** */
void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::begin_play);
    check(entity_radius > 0.f);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    entities.laser_cooldowns.set_tick_value(cooldown_tick_period);
    validate_array_sizes();
}
void Simulation::update_timers(float const) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::update_timers);

    entities.laser_cooldowns.tick();
}
void Simulation::move(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::move);

    rotate_instances(dt);
}
void Simulation::queue_commands() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::queue_commands);

    fire_lasers();
}
void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::end_tick);
}

/* **************************************** */
// Accessors
/* **************************************** */
auto Simulation::get_num_instances() const noexcept -> int32 {
    return entities.num();
}

/* **************************************** */
// Spawning
/* **************************************** */
void Simulation::spawn_instances(FVectors3f::ConstView const new_locations,
                                 TConstArrayView<float> const new_yaws,
                                 TConstArrayView<int32> const new_fire_point_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::spawn_instances);

    auto const n{new_locations.num()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_yaws),
        SANDBOX_NAMED_NUM(new_fire_point_indices),
    });

    entities.add_uninitialised(n);
    auto appended{entities.right(n)};
    ml::assign_from(appended.locations, new_locations);
    ml::copy_elements(appended.yaws, 0, new_yaws, 0, n);
    entities.laser_cooldowns.zero_last(n);
    ml::copy_elements(appended.next_fire_point_indices, 0, new_fire_point_indices, 0, n);

    checkCode(entities.validate_array_sizes());

    ml::entity_registry::EntityData entity_data;
    entity_data.add_uninitialised(n);
    ml::assign_from(entity_data.locations, new_locations);
    for (int32 i{}; i < n; ++i) {
        ml::assign(entity_data.rotations, i, FRotator3f{0.f, new_yaws[i], 0.f});
    }
    ml::fill(entity_data.velocities, 0.f);
    ml::fill(entity_data.radii, entity_radius);
    ml::fill(entity_data.healths, 1000000);
    ml::fill(entity_data.teams, ETestTeam::White);
    entity_data.set_all_entity_types(ETestEntityType::TubeSpinner);
    entity_data.set_all_alive();
    checkCode(entity_data.validate_array_sizes());

    auto new_entities{entity_registry.add_entities(entity_data.get_const_view())};

    for (int32 i{0}; i < n; ++i) {
        appended.handles[i] = new_entities.get_handle(i);
    }

    checkCode(validate_array_sizes());
}

/* **************************************** */
// Movement
/* **************************************** */
void Simulation::rotate_instances(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::rotate_instances);

    auto const speed{config.yaw_rotation_speed_degrees};
    auto const delta_yaw_degrees{dt * speed};

    ml::add_in_place(TArrayView<float>(entities.yaws), delta_yaw_degrees);
}

/* **************************************** */
// Firing
/* **************************************** */
void Simulation::fire_lasers() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::fire_lasers);

    if (fire_points_.empty()) {
        return;
    }

    auto const count{static_cast<std::size_t>(get_num_instances())};
    ml::simulation::lasers::FrameSpawnRequests new_lasers{&frame_memory_resource};
    ml::simulation::spinners::fire_lasers(
        {.locations = ml::to_native(entities.locations.get_const_view()),
         .yaws = {entities.yaws.GetData(), count},
         .handles = {entities.handles.GetData(), count},
         .next_fire_point_indices = {entities.next_fire_point_indices.GetData(), count},
         .cooldowns = entities.laser_cooldowns.get_view().native_view()},
        fire_points_,
        {.damage = config.laser.damage,
         .speed = config.laser.projectile_speed,
         .maximum_distance = config.laser.max_distance},
        frame_memory_resource,
        new_lasers);
    laser_simulation.queue_laser_spawns(new_lasers.get_const_view());
}

/* **************************************** */
// Checks
/* **************************************** */
void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
} // namespace ml::test_tube_spinners

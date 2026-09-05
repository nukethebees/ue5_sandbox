#include "SpaceGame/defences/spinners/TestTubeSpinnersSimulation.h"

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/simulation/LevelSimulationConfig.h>

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_math.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

namespace ml::test_tube_spinners {
void Simulation::set_config(FSpinnerSimulationConfig const& new_config) noexcept {
    config = new_config;
}

void Simulation::set_entity_registry(FTestEntityRegistry& new_registry) noexcept {
    entity_registry = &new_registry;
}

void Simulation::set_laser_simulation(ml::test_lasers::Simulation& new_simulation) noexcept {
    laser_simulation = &new_simulation;
}

void Simulation::clear_runtime_state() {
    ml::reset(entities, indices_ready_to_fire, new_lasers);
}

void Simulation::begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::begin_play);
    check(entity_registry);
    check(laser_simulation);
    check(entity_radius > 0.f);

    auto const cooldown_tick_period{
        simulation_clock.duration_to_tick_period(config.laser.fire_cooldown)};
    entities.laser_cooldowns.set_tick_value(cooldown_tick_period);
    validate_array_sizes();
}

void Simulation::bind_simulation_clock(FSimulationClock const& clock) noexcept {
    simulation_clock.bind(clock);
}

void Simulation::begin_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::begin_tick);
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

void Simulation::update_entity_registry() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::update_entity_registry);
}

void Simulation::end_tick() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::end_tick);

    ml::reset(indices_ready_to_fire, new_lasers);
}

auto Simulation::get_num_instances() const noexcept -> int32 {
    return entities.num();
}

void Simulation::spawn_instances(FVectors3f::ConstView const new_locations,
                                 TConstArrayView<float> const new_yaws,
                                 TConstArrayView<int32> const new_fire_point_indices) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::spawn_instances);

    auto const n{new_locations.num()};
    auto const existing_total{get_num_instances()};

    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(new_locations),
        SANDBOX_NAMED_NUM(new_yaws),
        SANDBOX_NAMED_NUM(new_fire_point_indices),
    });

    entities.handles.AddDefaulted(n);
    entities.locations.append_from(new_locations);
    entities.yaws.Append(new_yaws);
    entities.laser_cooldowns.add_zeroed(n);
    entities.next_fire_point_indices.Append(new_fire_point_indices);

    checkCode(entities.validate_array_sizes());

    ml::entity_registry::EntityData entity_data;
    entity_data.add_uninitialised(n);
    ml::assign_from(entity_data.locations, new_locations);
    ml::fill(entity_data.velocities, 0.f);
    ml::fill(entity_data.radii, entity_radius);
    ml::fill(entity_data.healths, 1000000);
    ml::fill(entity_data.teams, ETestTeam::White);
    entity_data.set_all_entity_types(ETestEntityType::TubeSpinner);
    entity_data.set_all_alive();
    checkCode(entity_data.validate_array_sizes());

    auto new_entities{entity_registry->add_entities(entity_data.get_const_view())};

    for (int32 i{0}; i < n; ++i) {
        entities.handles[i + existing_total] = new_entities.registry_handles[i];
    }

    checkCode(validate_array_sizes());
}

void Simulation::rotate_instances(float const dt) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::rotate_instances);

    auto const speed{config.yaw_rotation_speed_degrees};
    auto const delta_yaw_degrees{dt * speed};

    ml::add_in_place(TArrayView<float>(entities.yaws), delta_yaw_degrees);
}

void Simulation::fire_lasers() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::test_tube_spinners::Simulation::fire_lasers);

    auto const n{get_num_instances()};
    auto const& firing_point_offsets{config.fire_point_offsets};
    auto const n_firing_points{firing_point_offsets.Num()};
    auto const laser_damage{config.laser.damage};
    auto const laser_speed{config.laser.projectile_speed};
    auto const laser_max_distance{config.laser.max_distance};

    if (n_firing_points < 1) {
        return;
    }

    ml::reset(indices_ready_to_fire, new_lasers);

    for (int32 i{0}; i < n; ++i) {
        if (!entities.laser_cooldowns.is_ready(i)) {
            continue;
        }

        indices_ready_to_fire.Add(i);
        entities.laser_cooldowns.restart_counter(i);
    }

    auto const n_ready_to_fire{indices_ready_to_fire.Num()};
    ml::reserve(n_ready_to_fire, new_lasers);
    ml::add_uninitialised(n_ready_to_fire, new_lasers);

    for (int32 i{0}; i < n_ready_to_fire; ++i) {
        auto const index{indices_ready_to_fire[i]};

        auto const fire_point_index{entities.next_fire_point_indices[index]};
        auto const& offset{firing_point_offsets[fire_point_index]};

        auto const fire_point_location{offset.GetLocation()};
        ml::assign(new_lasers.locations,
                   i,
                   entities.locations.xs[index] + fire_point_location.X,
                   entities.locations.ys[index] + fire_point_location.Y,
                   entities.locations.zs[index] + fire_point_location.Z);

        auto const fire_point_rotation{offset.Rotator()};
        ml::assign(new_lasers.rotations,
                   i,
                   fire_point_rotation.Pitch,
                   fire_point_rotation.Yaw + entities.yaws[index],
                   fire_point_rotation.Roll);
        ml::assign(new_lasers.base_velocities, i, FVector3f::ZeroVector);

        new_lasers.damages[i] = laser_damage;
        new_lasers.speeds[i] = laser_speed;
        new_lasers.max_distances[i] = laser_max_distance;
        new_lasers.instigator_handles[i] = entities.handles[index];

        entities.next_fire_point_indices[index] = (fire_point_index + 1) % n_firing_points;
    }

    laser_simulation->queue_laser_spawns(new_lasers);
}

void Simulation::validate_array_sizes() const {
    entities.validate_array_sizes();
}
} // namespace ml::test_tube_spinners

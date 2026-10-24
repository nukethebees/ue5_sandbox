#include <ioj/sim/levels/initial_spawns.h>

#include <ioj/column_math.h>
#include <ioj/sim/levels/level_sim_init_data.h>

#include <cassert>
#include <limits>

namespace ioj::sim {
void initialise_spawns(LevelSimInitData& data,
                       std::span<InitialCapital const> capitals,
                       std::span<InitialTurret const> turrets,
                       std::span<InitialSpinner const> spinners) {
    auto& schedule{data.level_events.schedule};
    assert(schedule.capital_spawns.is_empty() && schedule.turret_spawns.is_empty() &&
           schedule.spinner_spawns.is_empty() && schedule.spawn_groups.is_empty());
    auto const count = [](std::size_t size) {
        assert(size <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
        return static_cast<std::int32_t>(size);
    };
    auto const capital_count{count(capitals.size())};
    auto const turret_count{count(turrets.size())};
    auto const spinner_count{count(spinners.size())};
    schedule.add_initial_spawn_group(EntityType::CapitalShip, 0, capital_count);
    schedule.add_initial_spawn_group(EntityType::Turret, 0, turret_count);
    schedule.add_initial_spawn_group(EntityType::TubeSpinner, 0, spinner_count);
    schedule.capital_spawns.add_uninitialised(capital_count);
    schedule.turret_spawns.add_uninitialised(turret_count);
    schedule.spinner_spawns.add_uninitialised(spinner_count);

    auto const capital_events{schedule.capital_spawns.get_view()};
    for (std::int32_t i{}; i < capital_count; ++i) {
        auto const& spawn{capitals[i]};
        assert(spawn.entity < data.level_events.initialisation.entity_count);
        assert(spawn.target == invalid_level_entity_index ||
               spawn.target < data.level_events.initialisation.entity_count);
        capital_events.entity_indices()[i] = spawn.entity;
        capital_events.target_entity_indices()[i] = spawn.target;
        set_vector(capital_events.view_locations(), i, spawn.location);
        set_rotation(capital_events.view_rotations(), i, spawn.rotation);
        capital_events.teams()[i] = spawn.team;
        capital_events.healths()[i] = spawn.health.value_or(data.capital_ships.max_health);
        capital_events.initial_fighter_spawn_delays()[i] = spawn.initial_spawn_delay.value_or(0.f);
        capital_events.fighter_spawn_cooldowns()[i] =
            spawn.spawn_cooldown.value_or(data.capital_ships.spawn_delay);
    }

    auto const turret_events{schedule.turret_spawns.get_view()};
    for (std::int32_t i{}; i < turret_count; ++i) {
        auto const& spawn{turrets[i]};
        assert(spawn.entity < data.level_events.initialisation.entity_count);
        turret_events.entity_indices()[i] = spawn.entity;
        set_vector(turret_events.view_locations(), i, spawn.location);
        set_rotation(turret_events.view_rotations(), i, spawn.rotation);
        turret_events.teams()[i] = spawn.team;
        turret_events.healths()[i] = spawn.health.value_or(data.turrets.max_health);
        turret_events.laser_damages()[i] = spawn.laser_damage.value_or(data.turrets.laser.damage);
    }

    auto const spinner_events{schedule.spinner_spawns.get_view()};
    for (std::int32_t i{}; i < spinner_count; ++i) {
        auto const& spawn{spinners[i]};
        assert(spawn.entity < data.level_events.initialisation.entity_count);
        spinner_events.entity_indices()[i] = spawn.entity;
        set_vector(spinner_events.view_locations(), i, spawn.location);
        spinner_events.yaws()[i] = spawn.yaw;
        spinner_events.initial_fire_point_indices()[i] = spawn.initial_fire_point;
    }
}
}

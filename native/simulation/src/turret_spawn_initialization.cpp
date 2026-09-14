#include "ioj/sim/turret_spawn_initialization.h"

#include <cassert>

namespace ioj::sim::turrets {
void initialize_spawned_turrets(SpawnInitializationView const turrets,
                                TurretSpawnDataConstView const spawns,
                                Vector3f const fire_point_offset,
                                std::int16_t const refresh_period,
                                std::int32_t& next_offset) {
    assert(refresh_period > 0);
    auto const count{turrets.locations.num()};
    assert(spawns.locations.num() == count);
    assert(turrets.fire_point_locations.num() == count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const location{spawns.locations[index]};
        turrets.locations.set(index, location);
        turrets.fire_point_locations.set(index, location + fire_point_offset);
        turrets.teams[element] = static_cast<std::byte>(spawns.teams[element]);
        turrets.healths[element] = spawns.healths[element];
        turrets.laser_damages[element] = spawns.laser_damages[element];
        turrets.refresh_periods[element] = refresh_period;
        turrets.refresh_remaining_ticks[element] = static_cast<std::int16_t>(next_offset);
        ++next_offset;
        if (next_offset == refresh_period) {
            next_offset = 0;
        }
    }
}
}

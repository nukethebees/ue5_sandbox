#pragma once
#include <ioj/sim/capital_ships/capital_entity_data.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/player/player_spatial_data.h>
#include <ioj/sim/spinners/spinner_entity_data.h>
#include <ioj/sim/turrets/turret_entity_data.h>

#include <optional>

namespace ioj::sim::collision {
struct CollisionGridEntities {
    CapitalEntityData::CompactConstView capitals;
    FighterEntityData::CompactConstView fighters;
    TurretEntityData::CompactConstView turrets;
    SpinnerEntityData::CompactConstView spinners;
    HealthTable const& health;
    std::optional<PlayerSpatialData> player{};
};
}

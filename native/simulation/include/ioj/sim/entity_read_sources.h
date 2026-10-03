#pragma once

#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/transform3d.h>
#include <ioj/sim/turret_entity_data.h>

namespace ioj::sim {
struct PlayerEntitySource {
    Transform3d const* transform{};
    ml::Vector3d const* velocity{};
    Team const* team{};
    EntityUniqueId id{};
};

// Keep owners rather than borrowed spans so presentation can acquire views after compaction.
struct EntityReadSources {
    CapitalEntityData const* capitals{};
    FighterEntityData const* fighters{};
    TurretEntityData const* turrets{};
    SpinnerEntityData const* spinners{};
    PlayerEntitySource player;
};
}

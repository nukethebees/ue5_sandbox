#pragma once

#include <SpaceGame/levels/LevelTypes.h>

#include <Misc/Optional.h>

namespace ml::level_archetype_detail {
enum class EResolvedArchetype : uint8 {
    PlayerFighter,
    CapitalShip,
    StaticTurret,
};

inline auto resolve(FEntityArchetypeId const id) -> TOptional<EResolvedArchetype> {
    if (id == level_archetypes::player_fighter) {
        return EResolvedArchetype::PlayerFighter;
    }
    if (id == level_archetypes::capital_ship) {
        return EResolvedArchetype::CapitalShip;
    }
    if (id == level_archetypes::static_turret) {
        return EResolvedArchetype::StaticTurret;
    }
    return NullOpt;
}
}

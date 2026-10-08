#pragma once

#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGameSimulation/levels/LevelTypes.h>

#include <Misc/Optional.h>

namespace ml {
enum class EResolvedLevelArchetype : uint8 {
    PlayerFighter,
    CapitalShip,
    StaticTurret,
};

inline auto resolve_level_archetype(FEntityArchetypeId const id)
    -> TOptional<EResolvedLevelArchetype> {
    if (id == level_archetypes::player_fighter) {
        return EResolvedLevelArchetype::PlayerFighter;
    }
    if (id == level_archetypes::capital_ship) {
        return EResolvedLevelArchetype::CapitalShip;
    }
    if (id == level_archetypes::static_turret) {
        return EResolvedLevelArchetype::StaticTurret;
    }
    return NullOpt;
}

inline auto to_level_archetype_id(EResolvedLevelArchetype const archetype) -> FEntityArchetypeId {
    switch (archetype) {
        case EResolvedLevelArchetype::PlayerFighter:
            return level_archetypes::player_fighter;
        case EResolvedLevelArchetype::CapitalShip:
            return level_archetypes::capital_ship;
        case EResolvedLevelArchetype::StaticTurret:
            return level_archetypes::static_turret;
    }
    checkNoEntry();
    return {};
}

}

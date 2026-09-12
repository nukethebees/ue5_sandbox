#pragma once

#include <SpaceGameSimulation/entities/TestTeam.h>
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

inline auto resolve_level_team(FLevelTeamId const id) -> TOptional<ETestTeam> {
    if (id == level_teams::white) {
        return ETestTeam::White;
    }
    if (id == level_teams::red) {
        return ETestTeam::Red;
    }
    if (id == level_teams::green) {
        return ETestTeam::Green;
    }
    if (id == level_teams::blue) {
        return ETestTeam::Blue;
    }
    if (id == level_teams::orange) {
        return ETestTeam::Orange;
    }
    if (id == level_teams::yellow) {
        return ETestTeam::Yellow;
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

inline auto to_level_team_id(ETestTeam const team) -> TOptional<FLevelTeamId> {
    switch (team) {
        case ETestTeam::White:
            return level_teams::white;
        case ETestTeam::Red:
            return level_teams::red;
        case ETestTeam::Green:
            return level_teams::green;
        case ETestTeam::Blue:
            return level_teams::blue;
        case ETestTeam::Orange:
            return level_teams::orange;
        case ETestTeam::Yellow:
            return level_teams::yellow;
        case ETestTeam::COUNT:
            return NullOpt;
    }
    return NullOpt;
}
}

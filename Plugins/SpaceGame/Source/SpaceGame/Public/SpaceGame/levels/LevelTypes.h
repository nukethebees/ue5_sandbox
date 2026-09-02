#pragma once

#include <CoreMinimal.h>

namespace ml {
struct SPACEGAME_API FLevelTeamId {
    FName value{NAME_None};

    auto operator==(FLevelTeamId const&) const -> bool = default;
};

inline auto GetTypeHash(FLevelTeamId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

struct SPACEGAME_API FLevelEntityId {
    FName value{NAME_None};

    auto operator==(FLevelEntityId const&) const -> bool = default;
    auto is_set() const noexcept -> bool { return !value.IsNone(); }
};

inline auto GetTypeHash(FLevelEntityId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

struct SPACEGAME_API FEntityArchetypeId {
    FName value{NAME_None};

    auto operator==(FEntityArchetypeId const&) const -> bool = default;
};

inline auto GetTypeHash(FEntityArchetypeId const& id) -> uint32 {
    return GetTypeHash(id.value);
}

namespace level_teams {
inline FLevelTeamId const white{FName{TEXT("white")}};
inline FLevelTeamId const red{FName{TEXT("red")}};
inline FLevelTeamId const green{FName{TEXT("green")}};
inline FLevelTeamId const blue{FName{TEXT("blue")}};
inline FLevelTeamId const orange{FName{TEXT("orange")}};
inline FLevelTeamId const yellow{FName{TEXT("yellow")}};
}

namespace level_archetypes {
inline FEntityArchetypeId const player_fighter{FName{TEXT("player-fighter")}};
inline FEntityArchetypeId const capital_ship{FName{TEXT("capital-ship")}};
inline FEntityArchetypeId const static_turret{FName{TEXT("static-turret")}};
}
}

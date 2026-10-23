#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_type.h>
#include <SpaceGamePresentation/entities/TestTeam.h>

#include <SandboxCoreEngine/enums.h>

#include <Containers/Array.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

namespace ioj::ship_hud {
inline constexpr int32 top_killer_inline_capacity{10};

struct FTopKillerEntry {
    bool operator==(FTopKillerEntry const&) const noexcept = default;

    sim::EntityId entity_id{};
    sim::EntityType entity_type{};
    ETestTeam team{};
    int32 kills{};
};

struct FTeamKillMatrix {
    static constexpr int32 team_count{enum_count<ETestTeam>()};
    static constexpr int32 entity_type_count{enum_count<sim::EntityType>()};
    static constexpr int32 count{team_count * entity_type_count};

    bool operator==(FTeamKillMatrix const&) const noexcept = default;

    static constexpr auto team_to_index(ETestTeam const team) -> int32 {
        return std::to_underlying(team);
    }
    static constexpr auto to_index(ETestTeam const team, sim::EntityType const entity_type)
        -> int32 {
        return team_to_index(team) * entity_type_count + std::to_underlying(entity_type);
    }

    auto get(ETestTeam const team, sim::EntityType const entity_type) const -> int32 {
        return kills[to_index(team, entity_type)];
    }
    auto get_team_kills(ETestTeam const team) const -> TConstArrayView<int32> {
        return {kills.GetData() + team_to_index(team) * entity_type_count, entity_type_count};
    }
    void set(ETestTeam const team, sim::EntityType const entity_type, int32 const kill_count) {
        kills[to_index(team, entity_type)] = kill_count;
    }
    void add(ETestTeam const team, sim::EntityType const entity_type, int32 const kill_count = 1) {
        kills[to_index(team, entity_type)] += kill_count;
    }

    TStaticArray<int32, count> kills{};
};
}

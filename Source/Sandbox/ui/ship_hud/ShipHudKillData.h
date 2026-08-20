#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <SandboxGameShared/utilities/enums.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

#include <utility>

namespace ml::ship_hud {
struct FTopKillerEntries : public ml::FSoAArrayMixin {
    static constexpr int32 minimum_size{10};

    bool operator==(FTopKillerEntries const& other) const noexcept {
        return entity_ids == other.entity_ids && entity_types == other.entity_types &&
               teams == other.teams && kills == other.kills;
    }

    void add(TestEntityUniqueId const entity_id,
             ETestEntityType const entity_type,
             ETestTeam const team,
             int32 const kill_count) {
        entity_ids.Add(entity_id);
        entity_types.Add(entity_type);
        teams.Add(team);
        kills.Add(kill_count);
    }

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(
            self.entity_ids, self.entity_types, self.teams, self.kills);
    }

    TArray<TestEntityUniqueId, TInlineAllocator<minimum_size>> entity_ids;
    TArray<ETestEntityType, TInlineAllocator<minimum_size>> entity_types;
    TArray<ETestTeam, TInlineAllocator<minimum_size>> teams;
    TArray<int32, TInlineAllocator<minimum_size>> kills;
};

struct FTeamKillMatrix {
    static constexpr int32 team_count{ml::EnumCountTrait<ETestTeam>::count_value};
    static constexpr int32 entity_type_count{ml::EnumCountTrait<ETestEntityType>::count_value};
    static constexpr int32 count{team_count * entity_type_count};

    bool operator==(FTeamKillMatrix const&) const noexcept = default;

    static constexpr auto team_to_index(ETestTeam const team) -> int32 {
        return std::to_underlying(team);
    }
    static constexpr auto to_index(ETestTeam const team, ETestEntityType const entity_type)
        -> int32 {
        return team_to_index(team) * entity_type_count + std::to_underlying(entity_type);
    }

    auto get(ETestTeam const team, ETestEntityType const entity_type) const -> int32 {
        return kills[to_index(team, entity_type)];
    }
    auto get_team_kills(ETestTeam const team) const -> TConstArrayView<int32> {
        return {kills.GetData() + team_to_index(team) * entity_type_count, entity_type_count};
    }
    void set(ETestTeam const team, ETestEntityType const entity_type, int32 const kill_count) {
        kills[to_index(team, entity_type)] = kill_count;
    }
    void add(ETestTeam const team, ETestEntityType const entity_type, int32 const kill_count = 1) {
        kills[to_index(team, entity_type)] += kill_count;
    }

    TStaticArray<int32, count> kills{};
};
}

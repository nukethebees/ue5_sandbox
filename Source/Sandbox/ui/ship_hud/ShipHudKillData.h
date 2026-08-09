#pragma once

#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>
#include <Sandbox/utilities/enums.h>

#include <Containers/StaticArray.h>
#include <CoreMinimal.h>

namespace ml::ship_hud {
struct FTopKillerEntry {
    bool operator==(FTopKillerEntry const&) const noexcept = default;

    TestEntityUniqueId entity_id{};
    ETestEntityType entity_type{ETestEntityType::PlayerShip};
    ETestTeam team{ETestTeam::White};
    int32 kills{0};
};

struct FTeamKillMatrixRow {
    static constexpr int32 entity_type_count{ml::EnumCountTrait<ETestEntityType>::count_value};

    bool operator==(FTeamKillMatrixRow const&) const noexcept = default;

    ETestTeam killer_team{ETestTeam::White};
    TStaticArray<int32, entity_type_count> kills_by_victim_type{};
};
}

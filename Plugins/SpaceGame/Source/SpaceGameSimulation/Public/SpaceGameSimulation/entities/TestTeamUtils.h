#pragma once

#include <SpaceGameSimulation/entities/TestTeam.h>

#include <SandboxCore/fixed_array.h>
#include <SandboxCoreEngine/enums.h>

#include <Containers/StaticArray.h>

namespace ml {
inline constexpr int32 test_team_count{EnumCountTrait<ETestTeam>::count_value};

template <typename T>
using TStaticTeamArray = TStaticArray<T, test_team_count>;

using FTestTeamList = TFixedArray<ETestTeam, test_team_count>;

constexpr bool is_valid(ETestTeam const team) {
    using T = uint8;
    return static_cast<T>(team) < static_cast<T>(ETestTeam::COUNT);
}
}

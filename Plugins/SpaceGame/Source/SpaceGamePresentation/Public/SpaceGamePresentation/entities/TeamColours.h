#pragma once

#include <SpaceGamePresentation/entities/TestTeam.h>

#include <Containers/StaticArray.h>
#include <Math/Color.h>

#include <utility>

namespace ioj {
inline constexpr int32 test_team_count{ioj::enum_count<ETestTeam>()};

template <typename T>
using TStaticTeamArray = TStaticArray<T, test_team_count>;

constexpr bool is_valid(ETestTeam const team) {
    return std::to_underlying(team) < ioj::enum_count<ETestTeam>();
}
}

struct FTeamColours {
    FTeamColours() {
        for (auto& colour : colours) {
            colour = FLinearColor{1.f, 0.f, 1.f, 1.f};
        }
    }
    auto operator[](ETestTeam team) const -> FLinearColor const& {
        return colours[std::to_underlying(team)];
    }
    auto operator[](ETestTeam team) -> FLinearColor& { return colours[std::to_underlying(team)]; }
    ioj::TStaticTeamArray<FLinearColor> colours;
};

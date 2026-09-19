#pragma once

#include <SpaceGamePresentation/entities/TestTeam.h>

#include <Containers/StaticArray.h>
#include <Math/Color.h>

#include <utility>

namespace ml {
inline constexpr int32 test_team_count{std::to_underlying(ETestTeam::COUNT)};

template <typename T>
using TStaticTeamArray = TStaticArray<T, test_team_count>;

constexpr bool is_valid(ETestTeam const team) {
    return std::to_underlying(team) < std::to_underlying(ETestTeam::COUNT);
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
    ml::TStaticTeamArray<FLinearColor> colours;
};

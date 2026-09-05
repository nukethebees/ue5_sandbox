#pragma once

#include <Containers/StaticArray.h>
#include <Math/Color.h>
#include <SpaceGame/entities/TestTeam.h>
#include <utility>

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
    TStaticArray<FLinearColor, std::to_underlying(ETestTeam::COUNT)> colours;
};

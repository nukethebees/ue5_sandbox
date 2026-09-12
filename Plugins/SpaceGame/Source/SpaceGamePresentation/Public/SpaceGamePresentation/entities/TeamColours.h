#pragma once

#include <Math/Color.h>
#include <SpaceGameSimulation/entities/TestTeamUtils.h>

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
    ml::TStaticTeamArray<FLinearColor> colours;
};

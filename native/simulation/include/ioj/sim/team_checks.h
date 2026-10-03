#pragma once

#include <ioj/sim/team.h>

#include <algorithm>
#include <span>

namespace ioj::sim {
[[nodiscard]] inline auto check_valid_teams(std::span<Team const> const teams) noexcept -> bool {
    return std::ranges::all_of(teams, [](Team const team) { return team < Team::COUNT; });
}
} // namespace ioj::sim

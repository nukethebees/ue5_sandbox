#pragma once

#include <ioj/sim/team.h>

#include <algorithm>
#include <span>

namespace ioj::sim {
[[nodiscard]] inline auto check_valid_teams(std::span<Team const> const teams) noexcept -> bool {
    return std::ranges::all_of(
        teams, [](Team const team) { return std::to_underlying(team) < ml::enum_count<Team>(); });
}
} // namespace ioj::sim

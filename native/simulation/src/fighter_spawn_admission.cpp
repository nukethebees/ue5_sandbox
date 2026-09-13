#include "sandbox/simulation/fighter_spawn_admission.h"

#include <algorithm>
#include <cassert>
#include <limits>

namespace ml::simulation::fighters {
auto admit_spawns(std::span<std::byte const> const teams,
                  std::span<std::uint8_t const> const participant_mask,
                  std::span<std::int32_t> const remaining_team_capacity) -> SpawnAdmission {
    assert(participant_mask.size() == remaining_team_capacity.size());
    assert(teams.size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
    if (teams.empty()) {
        return {};
    }

    auto const team{teams.front()};
    auto const team_index{std::to_integer<std::size_t>(team)};
    if (team_index >= participant_mask.size() || participant_mask[team_index] == 0) {
        return {SpawnAdmissionStatus::InvalidTeam, 0};
    }

    for (auto const queued_team : teams) {
        if (queued_team != team) {
            return {SpawnAdmissionStatus::MixedTeams, 0};
        }
    }

    auto& capacity{remaining_team_capacity[team_index]};
    assert(capacity >= 0);
    auto const accepted_count{std::min(static_cast<std::int32_t>(teams.size()), capacity)};
    capacity -= accepted_count;
    return {SpawnAdmissionStatus::Accepted, accepted_count};
}
}

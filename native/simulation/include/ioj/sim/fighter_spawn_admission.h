#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim::fighters {
enum class SpawnAdmissionStatus : std::uint8_t {
    Accepted,
    InvalidTeam,
    MixedTeams,
};

struct SpawnAdmission {
    SpawnAdmissionStatus status{SpawnAdmissionStatus::Accepted};
    std::int32_t accepted_count{};
};

[[nodiscard]] auto admit_spawns(std::span<std::byte const> teams,
                                std::span<std::uint8_t const> participant_mask,
                                std::span<std::int32_t> remaining_team_capacity) -> SpawnAdmission;
}

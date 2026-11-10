#pragma once

#include "ioj/sim/entities/team_entity_array.h"

#include <cstdint>

namespace ioj::sim {
class CombatStatistics {
  public:
    void reset() noexcept;

    void record_spawn(Team team, EntityType type, bool alive) noexcept;
    void apply_alive_transition(
        Team old_team, Team new_team, EntityType type, bool old_alive, bool new_alive) noexcept;
    void record_kill(Team killer_team) noexcept;

    [[nodiscard]] auto alive_count() const noexcept -> std::int32_t;
    [[nodiscard]] auto count_alive_per_team_and_type() const noexcept -> EntityCounts const&;
    [[nodiscard]] auto kill_count(Team team) const noexcept -> std::int32_t;
  private:
    void adjust_alive_count(Team team, EntityType type, std::int32_t delta) noexcept;

    EntityCounts alive_counts_{};
    TeamCounts kill_counts_{};
};
} // namespace ioj::sim

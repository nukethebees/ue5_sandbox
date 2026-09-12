#pragma once

#include "sandbox/simulation/entity_telemetry.h"

#include <cstdint>

namespace ml::simulation {
class EntityRegistryStatistics {
  public:
    void reset() noexcept;

    void record_spawn(Team team, EntityType type, bool alive) noexcept;
    void apply_alive_transition(
        Team old_team, Team new_team, EntityType type, bool old_alive, bool new_alive) noexcept;
    void record_destroyed(Team team, EntityType type) noexcept;
    void record_kill(Team killer_team, EntityType killer_type, Team victim_team) noexcept;
    void record_damage_received(Team victim_team, EntityType victim_type, double damage) noexcept;
    void record_hit(Team attacker_team, EntityType attacker_type, double damage) noexcept;
    void record_shot(Team attacker_team, EntityType attacker_type) noexcept;

    [[nodiscard]] auto alive_count() const noexcept -> std::int32_t;
    [[nodiscard]] auto cumulative_kill_count() const noexcept -> std::int32_t;
    [[nodiscard]] auto count_alive(EntityType type) const noexcept -> std::int32_t;
    [[nodiscard]] auto count_alive_per_team() const noexcept -> telemetry::TeamCounts;
    [[nodiscard]] auto count_alive_per_team_and_type() const noexcept
        -> telemetry::EntityCounts const&;
    [[nodiscard]] auto count_alive_not_on_team(Team team) const noexcept -> std::int32_t;
    [[nodiscard]] auto combat_telemetry() const noexcept
        -> telemetry::CombatTelemetryCounters const&;
  private:
    void adjust_alive_count(Team team, EntityType type, std::int32_t delta) noexcept;

    telemetry::EntityCounts alive_counts_{};
    std::int32_t alive_count_{};
    std::int32_t cumulative_kill_count_{};
    telemetry::CombatTelemetryCounters combat_telemetry_{};
};
} // namespace ml::simulation

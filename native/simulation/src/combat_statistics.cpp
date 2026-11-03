#include "ioj/sim/combat_statistics.h"

#include <cassert>
#include <numeric>

namespace ioj::sim {
namespace {
constexpr auto is_valid(Team const team) noexcept -> bool {
    return std::to_underlying(team) < telemetry::team_count;
}

[[maybe_unused]] constexpr auto is_valid(EntityType const type) noexcept -> bool {
    return std::to_underlying(type) < telemetry::entity_type_count;
}
} // namespace

void CombatStatistics::reset() noexcept {
    alive_counts_ = {};
    combat_telemetry_ = {};
}

void CombatStatistics::record_spawn(Team const team,
                                    EntityType const type,
                                    bool const alive) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    ++combat_telemetry_.spawned[team][type];
    if (alive) {
        adjust_alive_count(team, type, 1);
    }
}

void CombatStatistics::apply_alive_transition(Team const old_team,
                                              Team const new_team,
                                              EntityType const type,
                                              bool const old_alive,
                                              bool const new_alive) noexcept {
    if (old_alive && (!new_alive || old_team != new_team)) {
        adjust_alive_count(old_team, type, -1);
    }
    if (new_alive && (!old_alive || old_team != new_team)) {
        adjust_alive_count(new_team, type, 1);
    }
}

void CombatStatistics::record_destroyed(Team const team, EntityType const type) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    ++combat_telemetry_.destroyed[team][type];
}

void CombatStatistics::record_kill(Team const killer_team, EntityType const killer_type) noexcept {
    assert(is_valid(killer_team));
    assert(is_valid(killer_type));

    ++combat_telemetry_.kills[killer_team][killer_type];
}

void CombatStatistics::record_hit(Team const attacker_team,
                                  EntityType const attacker_type,
                                  double const damage) noexcept {
    assert(is_valid(attacker_team));
    assert(is_valid(attacker_type));

    ++combat_telemetry_.hits[attacker_team][attacker_type];
    combat_telemetry_.damage_dealt[attacker_team][attacker_type] += damage;
}

void CombatStatistics::record_shot(Team const attacker_team,
                                   EntityType const attacker_type) noexcept {
    assert(is_valid(attacker_team));
    assert(is_valid(attacker_type));

    ++combat_telemetry_.shots[attacker_team][attacker_type];
}

auto CombatStatistics::alive_count() const noexcept -> std::int32_t {
    std::int32_t total{};
    for (auto const& counts : alive_counts_) {
        total += std::accumulate(counts.begin(), counts.end(), std::int32_t{});
    }
    return total;
}

auto CombatStatistics::cumulative_kill_count() const noexcept -> std::int32_t {
    std::uint64_t total{};
    for (auto const& counts : combat_telemetry_.kills) {
        total += std::accumulate(counts.begin(), counts.end(), std::uint64_t{});
    }
    return static_cast<std::int32_t>(total);
}

auto CombatStatistics::count_alive(EntityType const type) const noexcept -> std::int32_t {
    assert(is_valid(type));

    auto total{std::int32_t{}};
    for (auto const& team_counts : alive_counts_) {
        total += team_counts[type];
    }
    return total;
}

auto CombatStatistics::count_alive_per_team() const noexcept -> telemetry::TeamCounts {
    telemetry::TeamCounts counts{};
    for (auto const team : EnumTraits<Team>::values) {
        auto const& type_counts{alive_counts_[team]};
        counts[team] = std::accumulate(type_counts.begin(), type_counts.end(), std::int32_t{});
    }
    return counts;
}

auto CombatStatistics::count_alive_per_team_and_type() const noexcept
    -> telemetry::EntityCounts const& {
    return alive_counts_;
}

auto CombatStatistics::count_alive_not_on_team(Team const team) const noexcept -> std::int32_t {
    if (!is_valid(team)) {
        return alive_count();
    }

    auto const& counts{alive_counts_[team]};
    return alive_count() - std::accumulate(counts.begin(), counts.end(), std::int32_t{});
}

auto CombatStatistics::combat_telemetry() const noexcept
    -> telemetry::CombatTelemetryCounters const& {
    return combat_telemetry_;
}

void CombatStatistics::adjust_alive_count(Team const team,
                                          EntityType const type,
                                          std::int32_t const delta) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    auto& count{alive_counts_[team][type]};
    count += delta;
    assert(count >= 0);
}
} // namespace ioj::sim

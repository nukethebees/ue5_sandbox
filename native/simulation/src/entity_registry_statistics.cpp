#include "sandbox/simulation/entity_registry_statistics.h"

#include <cassert>
#include <numeric>
#include <utility>

namespace ml::simulation {
namespace {
constexpr auto is_valid(Team const team) noexcept -> bool {
    return std::to_underlying(team) < telemetry::team_count;
}

[[maybe_unused]] constexpr auto is_valid(EntityType const type) noexcept -> bool {
    return std::to_underlying(type) < telemetry::entity_type_count;
}
} // namespace

void EntityRegistryStatistics::reset() noexcept {
    alive_counts_ = {};
    alive_count_ = 0;
    cumulative_kill_count_ = 0;
    combat_telemetry_ = {};
}

void EntityRegistryStatistics::record_spawn(Team const team,
                                            EntityType const type,
                                            bool const alive) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    ++combat_telemetry_.spawned[std::to_underlying(team)][std::to_underlying(type)];
    if (alive) {
        adjust_alive_count(team, type, 1);
    }
}

void EntityRegistryStatistics::apply_alive_transition(Team const old_team,
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

void EntityRegistryStatistics::record_destroyed(Team const team, EntityType const type) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    auto const team_index{std::to_underlying(team)};
    auto const type_index{std::to_underlying(type)};
    ++combat_telemetry_.destroyed[team_index][type_index];
    ++combat_telemetry_.losses[team_index][type_index];
}

void EntityRegistryStatistics::record_kill(Team const killer_team,
                                           EntityType const killer_type,
                                           Team const victim_team) noexcept {
    assert(is_valid(killer_team));
    assert(is_valid(killer_type));
    assert(is_valid(victim_team));

    ++cumulative_kill_count_;
    ++combat_telemetry_.kills[std::to_underlying(killer_team)][std::to_underlying(killer_type)];
    ++combat_telemetry_
          .kill_matrix[std::to_underlying(killer_team)][std::to_underlying(victim_team)];
}

void EntityRegistryStatistics::record_damage_received(Team const victim_team,
                                                      EntityType const victim_type,
                                                      double const damage) noexcept {
    assert(is_valid(victim_team));
    assert(is_valid(victim_type));

    combat_telemetry_
        .damage_received[std::to_underlying(victim_team)][std::to_underlying(victim_type)] +=
        damage;
}

void EntityRegistryStatistics::record_hit(Team const attacker_team,
                                          EntityType const attacker_type,
                                          double const damage) noexcept {
    assert(is_valid(attacker_team));
    assert(is_valid(attacker_type));

    auto const team_index{std::to_underlying(attacker_team)};
    auto const type_index{std::to_underlying(attacker_type)};
    ++combat_telemetry_.hits[team_index][type_index];
    combat_telemetry_.damage_dealt[team_index][type_index] += damage;
}

void EntityRegistryStatistics::record_shot(Team const attacker_team,
                                           EntityType const attacker_type) noexcept {
    assert(is_valid(attacker_team));
    assert(is_valid(attacker_type));

    ++combat_telemetry_.shots[std::to_underlying(attacker_team)][std::to_underlying(attacker_type)];
}

auto EntityRegistryStatistics::alive_count() const noexcept -> std::int32_t {
    return alive_count_;
}

auto EntityRegistryStatistics::cumulative_kill_count() const noexcept -> std::int32_t {
    return cumulative_kill_count_;
}

auto EntityRegistryStatistics::count_alive(EntityType const type) const noexcept -> std::int32_t {
    assert(is_valid(type));

    auto total{std::int32_t{}};
    auto const type_index{std::to_underlying(type)};
    for (auto const& team_counts : alive_counts_) {
        total += team_counts[type_index];
    }
    return total;
}

auto EntityRegistryStatistics::count_alive_per_team() const noexcept -> telemetry::TeamCounts {
    telemetry::TeamCounts counts{};
    for (std::size_t team_index{}; team_index < telemetry::team_count; ++team_index) {
        auto const& type_counts{alive_counts_[team_index]};
        counts[team_index] =
            std::accumulate(type_counts.begin(), type_counts.end(), std::int32_t{});
    }
    return counts;
}

auto EntityRegistryStatistics::count_alive_per_team_and_type() const noexcept
    -> telemetry::EntityCounts const& {
    return alive_counts_;
}

auto EntityRegistryStatistics::count_alive_not_on_team(Team const team) const noexcept
    -> std::int32_t {
    if (!is_valid(team)) {
        return alive_count_;
    }

    auto const& counts{alive_counts_[std::to_underlying(team)]};
    return alive_count_ - std::accumulate(counts.begin(), counts.end(), std::int32_t{});
}

auto EntityRegistryStatistics::combat_telemetry() const noexcept
    -> telemetry::CombatTelemetryCounters const& {
    return combat_telemetry_;
}

void EntityRegistryStatistics::adjust_alive_count(Team const team,
                                                  EntityType const type,
                                                  std::int32_t const delta) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    auto& count{alive_counts_[std::to_underlying(team)][std::to_underlying(type)]};
    count += delta;
    alive_count_ += delta;
    assert(count >= 0);
    assert(alive_count_ >= 0);
}
} // namespace ml::simulation

#include "ioj/sim/combat_statistics.h"

#include <cassert>
#include <numeric>

namespace ioj::sim {
namespace {
constexpr auto is_valid(Team const team) noexcept -> bool {
    return std::to_underlying(team) < enum_count<Team>();
}

[[maybe_unused]] constexpr auto is_valid(EntityType const type) noexcept -> bool {
    return std::to_underlying(type) < enum_count<EntityType>();
}
} // namespace

void CombatStatistics::reset() noexcept {
    alive_counts_ = {};
    kill_counts_ = {};
}

void CombatStatistics::record_spawn(Team const team,
                                    EntityType const type,
                                    bool const alive) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

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

void CombatStatistics::record_kill(Team const killer_team) noexcept {
    assert(is_valid(killer_team));

    ++kill_counts_[killer_team];
}

auto CombatStatistics::alive_count() const noexcept -> std::int32_t {
    return std::accumulate(alive_counts_.begin(), alive_counts_.end(), std::int32_t{});
}

auto CombatStatistics::cumulative_kill_count() const noexcept -> std::int32_t {
    return std::accumulate(kill_counts_.begin(), kill_counts_.end(), std::int32_t{});
}

auto CombatStatistics::count_alive(EntityType const type) const noexcept -> std::int32_t {
    assert(is_valid(type));

    auto total{std::int32_t{}};
    for (auto const team : EnumTraits<Team>::values) {
        total += alive_counts_[team, type];
    }
    return total;
}

auto CombatStatistics::count_alive_per_team() const noexcept -> TeamCounts {
    TeamCounts counts{};
    for (auto const team : EnumTraits<Team>::values) {
        for (auto const type : EnumTraits<EntityType>::values) {
            counts[team] += alive_counts_[team, type];
        }
    }
    return counts;
}

auto CombatStatistics::count_alive_per_team_and_type() const noexcept -> EntityCounts const& {
    return alive_counts_;
}

auto CombatStatistics::kill_count(Team const team) const noexcept -> std::int32_t {
    return kill_counts_[team];
}

void CombatStatistics::adjust_alive_count(Team const team,
                                          EntityType const type,
                                          std::int32_t const delta) noexcept {
    assert(is_valid(team));
    assert(is_valid(type));

    auto& count{alive_counts_[team, type]};
    count += delta;
    assert(count >= 0);
}
} // namespace ioj::sim

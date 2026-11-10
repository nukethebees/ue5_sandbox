#include <ioj/sim/entities/entity_ledger.h>

#include <ioj/diagnostics.h>

#include <cassert>
#include <format>
#include <ranges>

namespace ioj::sim {
EntityLedger::EntityLedger(std::pmr::memory_resource& memory)
    : history_{memory}
    , ids_{memory}
    , frame_deaths_{{std::pmr::vector<HistoryIndex>{&memory},
                     std::pmr::vector<HistoryIndex>{&memory},
                     std::pmr::vector<HistoryIndex>{&memory},
                     std::pmr::vector<HistoryIndex>{&memory},
                     std::pmr::vector<HistoryIndex>{&memory}}} {}

void EntityLedger::reset() {
    history_.reset();
    ids_.reset();
    statistics_.reset();
    reset_frame_output();
}
void EntityLedger::reset_frame_output() {
    for (auto& deaths : frame_deaths_) {
        deaths.clear();
    }
}
auto EntityLedger::require_history_index(EntityId const id) const -> std::uint32_t {
    auto const index{ids_.history_index(id)};
    if (index == EntityIdAllocator::invalid_index) {
        fatal_error(
            std::format("Combat accounting references unknown entity ID {}", id.raw_value()));
    }
    return index;
}
template <typename Teams, typename Healths>
void EntityLedger::record_spawns_impl(EntityType const type,
                                      Teams const& teams,
                                      Healths const& healths,
                                      std::span<EntityId> const ids) {
    auto const count{ids.size()};
    assert(std::ranges::size(teams) == count && std::ranges::size(healths) == count);
    assert(std::in_range<EntityHistory::size_type>(count));
    auto const first{history_.num()};
    history_.add_defaulted(static_cast<EntityHistory::size_type>(count));
    auto const history{history_.right(static_cast<EntityHistory::size_type>(count))};
    auto const history_teams{history.teams()};
    auto const life_states{history.life_state()};

    for (std::size_t index{}; index < count; ++index) {
        auto const alive{is_alive(healths[index])};
        auto const id{ids_.allocate(type, first + static_cast<HistoryIndex>(index))};
        ids[index] = id;
        history_teams[index] = teams[index];
        life_states[index] = alive ? LifeState::Alive : LifeState::Unknown;
        statistics_.record_spawn(teams[index], type, alive);
    }
}
void EntityLedger::record_spawns(EntityType const type,
                                 std::span<Team const> const teams,
                                 std::span<Health const> const healths,
                                 std::span<EntityId> const ids) {
    record_spawns_impl(type, teams, healths, ids);
}
void EntityLedger::record_spawns(EntityType const type,
                                 std::span<Team const> const teams,
                                 Health const health,
                                 std::span<EntityId> const ids) {
    record_spawns_impl(type, teams, std::views::repeat(health, ids.size()), ids);
}
void EntityLedger::record_spawns(EntityType const type,
                                 Team const team,
                                 Health const health,
                                 std::span<EntityId> const ids) {
    record_spawns_impl(
        type, std::views::repeat(team, ids.size()), std::views::repeat(health, ids.size()), ids);
}
void EntityLedger::record_status(std::span<EntityId const> const ids,
                                 std::span<Team const> const teams,
                                 std::span<Health const> const healths) {
    auto const count{ids.size()};
    assert(teams.size() == count && healths.size() == count);
    auto const history{history_.view()};
    auto const history_teams{history.teams()};
    auto const life_states{history.life_state()};

    for (std::size_t index{}; index < count; ++index) {
        auto const id{ids[index]};
        auto const row{require_history_index(id)};
        auto const alive{is_alive(healths[index])};
        auto const old_alive{life_states[row] == LifeState::Alive};
        statistics_.apply_alive_transition(
            history_teams[row], teams[index], id.entity_type(), old_alive, alive);
        history_teams[row] = teams[index];
        if (alive) {
            life_states[row] = LifeState::Alive;
        } else if (old_alive) {
            life_states[row] = LifeState::Unknown;
        }
    }
}
void EntityLedger::record_deaths(EntityDeathInfo::ConstView const deaths, SimTick const tick) {
    auto const count{deaths.num()};
    auto const victims{deaths.victims()};
    auto const killers{deaths.killers()};
    auto const reasons{deaths.reasons()};
    auto const locations{deaths.locations()};
    auto const history{history_.view()};
    auto const teams{history.teams()};
    auto const life_states{history.life_state()};
    auto const death_ticks{history.death_ticks()};
    auto const death_locations{history.death_locations()};
    auto const kills{history.kills()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const victim{victims[index]};
        auto const row{require_history_index(victim)};
        auto const team{teams[row]};
        auto const alive{life_states[row] == LifeState::Alive};
        statistics_.apply_alive_transition(team, team, victim.entity_type(), alive, false);
        life_states[row] = static_cast<LifeState>(reasons[index]);
        death_ticks[row] = tick;
        death_locations[row] = locations[index];
        frame_deaths_[victim.entity_type()].push_back(row);
        auto const killer{killers[index]};
        if (!killer.is_valid()) {
            continue;
        }
        auto const killer_row{require_history_index(killer)};
        ++kills[killer_row];
        statistics_.record_kill(teams[killer_row]);
    }
}

auto EntityLedger::kills(EntityId const id) const -> std::uint32_t {
    return unique_entities().kills()[require_history_index(id)];
}
}

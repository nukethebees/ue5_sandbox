#include <ioj/sim/entities/entity_ledger.h>

#include <ioj/diagnostics.h>

#include <cassert>
#include <format>
#include <ranges>

namespace ioj::sim {
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
    auto const history_ids{history.entity_ids()};
    auto const history_types{history.entity_types()};
    auto const history_teams{history.teams()};
    auto const life_states{history.life_state()};

    for (std::size_t index{}; index < count; ++index) {
        auto const alive{is_alive(healths[index])};
        auto const id{ids_.allocate(type, first + static_cast<HistoryIndex>(index))};
        ids[index] = id;
        history_ids[index] = id;
        history_types[index] = type;
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
    auto const history{history_.get_view()};
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
    auto const history{history_.get_view()};
    auto const teams{history.teams()};
    auto const life_states{history.life_state()};
    auto const death_ticks{history.death_ticks()};
    auto const death_locations{history.death_locations()};
    auto const killed_by{history.killed_by()};
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
        frame_deaths_[std::to_underlying(victim.entity_type())].push_back(row);
        statistics_.record_destroyed(team, victim.entity_type());
        auto const killer{killers[index]};
        if (!killer.is_valid()) {
            continue;
        }
        auto const killer_row{require_history_index(killer)};
        killed_by[row] = killer;
        ++kills[killer_row];
        statistics_.record_kill(teams[killer_row], killer.entity_type(), team);
    }
}
void EntityLedger::record_damage(DirectDamageEvents::ConstView const events,
                                 std::span<Health const> const applied_damage) {
    auto const count{events.num()};
    assert(applied_damage.size() == count);
    auto const victims{events.damaged_entities()};
    auto const attackers{events.instigators()};
    auto const history{get_unique_entities()};
    auto const teams{history.teams()};
    [[maybe_unused]] auto const life_states{history.life_state()};

    for (std::uint32_t index{}; index < count; ++index) {
        auto const damage{applied_damage[index]};
        assert(damage >= 0);
        if (damage == 0) {
            continue;
        }

        auto const victim{victims[index]};
        auto const victim_row{require_history_index(victim)};
        assert(life_states[victim_row] == LifeState::Alive);
        statistics_.record_damage_received(
            teams[victim_row], victim.entity_type(), static_cast<double>(damage));
        auto const attacker{attackers[index]};
        if (attacker.is_valid()) {
            auto const attacker_row{require_history_index(attacker)};
            statistics_.record_hit(
                teams[attacker_row], attacker.entity_type(), static_cast<double>(damage));
        }
    }
}
void EntityLedger::record_shots(std::span<EntityId const> const instigators) {
    auto const teams{get_unique_entities().teams()};
    for (auto const id : instigators) {
        if (id.is_valid()) {
            statistics_.record_shot(teams[require_history_index(id)], id.entity_type());
        }
    }
}
auto EntityLedger::get_kills(EntityId const id) const -> std::uint32_t {
    return get_unique_entities().kills()[require_history_index(id)];
}
}

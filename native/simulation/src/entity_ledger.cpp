#include <ioj/sim/entity_ledger.h>

#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>

namespace ioj::sim {
void EntityLedger::reset() {
    history_.reset();
    ids_.reset();
    statistics_.reset();
}
auto EntityLedger::require_history_index(EntityUniqueId const id) const -> std::int32_t {
    auto const index{ids_.history_index(id)};
    if (index < 0) {
        ml::fatal_error(
            std::format("Combat accounting references unknown entity ID {}", id.raw_value()));
    }
    return index;
}
auto EntityLedger::record_spawn(EntityType const type, Team const team, bool const alive)
    -> EntityUniqueId {
    auto const row{history_.num()};
    auto const id{ids_.allocate(type, row)};
    history_.add_defaulted(1);
    auto const history{history_.get_view().columns()};
    history.entity_ids[row] = id;
    history.entity_types[row] = type;
    history.teams[row] = team;
    history.life_state[row] = alive ? LifeState::Alive : LifeState::Unknown;
    statistics_.record_spawn(team, type, alive);
    return id;
}
void EntityLedger::record_status(EntityUniqueId const id, Team const team, bool const alive) {
    auto const row{require_history_index(id)};
    auto const history{history_.get_view().columns()};
    auto const old_alive{history.life_state[row] == LifeState::Alive};
    statistics_.apply_alive_transition(
        history.teams[row], team, id.entity_type(), old_alive, alive);
    history.teams[row] = team;
    if (alive) {
        history.life_state[row] = LifeState::Alive;
    } else if (old_alive) {
        history.life_state[row] = LifeState::Unknown;
    }
}
void EntityLedger::record_death(EntityUniqueId const victim,
                                EntityUniqueId const killer,
                                DeathReason const reason) {
    auto const row{require_history_index(victim)};
    auto const history{history_.get_view().columns()};
    auto const team{history.teams[row]};
    record_status(victim, team, false);
    history.life_state[row] = static_cast<LifeState>(reason);
    statistics_.record_destroyed(team, victim.entity_type());
    if (!killer.is_valid()) {
        return;
    }
    auto const killer_row{require_history_index(killer)};
    history.killed_by[row] = killer;
    ++history.kills[killer_row];
    statistics_.record_kill(history.teams[killer_row], killer.entity_type(), team);
}
void EntityLedger::record_damage(DirectDamageEventsConstView const events) {
    auto const history{get_unique_entities()};
    auto const count{events.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const victim{events.damaged_entities[i]};
        auto const row{require_history_index(victim)};
        auto const damage{static_cast<double>(events.damage_amounts[i])};
        statistics_.record_damage_received(history.teams[row], victim.entity_type(), damage);
        auto const attacker{events.instigators[i]};
        if (attacker.is_valid()) {
            auto const attacker_row{require_history_index(attacker)};
            statistics_.record_hit(history.teams[attacker_row], attacker.entity_type(), damage);
        }
    }
}
void EntityLedger::record_shots(std::span<EntityUniqueId const> const instigators) {
    auto const history{get_unique_entities()};
    for (auto const id : instigators) {
        if (id.is_valid()) {
            statistics_.record_shot(history.teams[require_history_index(id)], id.entity_type());
        }
    }
}
auto EntityLedger::get_kills(EntityUniqueId const id) const -> std::uint32_t {
    return get_unique_entities().kills[require_history_index(id)];
}
}

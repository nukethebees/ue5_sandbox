#include <ioj/sim/agent_accessor.h>

#include <algorithm>
#include <cassert>
#include <numeric>

namespace ioj::sim {
namespace agent_accessor_detail {
struct TargetState {
    Vector3f location{};
    Vector3f velocity{};
    Team team{};
    bool alive{};
};
}

void AgentAccessor::gather_targets(std::span<EntityUniqueId const> const ids,
                                   std::span<std::int32_t> const order,
                                   AgentTargetView const output) const {
    auto const count{ids.size()};
    assert(order.size() == count && output.alive.size() == count);
    assert(static_cast<std::size_t>(output.locations.num()) == count);
    assert(output.velocities.is_empty() ||
           static_cast<std::size_t>(output.velocities.num()) == count);
    assert(output.teams.empty() || output.teams.size() == count);
    std::iota(order.begin(), order.end(), 0);
    std::ranges::sort(order, {}, [&](std::int32_t const row) { return ids[row]; });
    auto const scatter{
        [&](std::int32_t const row, agent_accessor_detail::TargetState const& state) {
            output.locations.set(row, state.location);
            if (!output.velocities.is_empty()) {
                output.velocities.set(row, state.velocity);
            }
            if (!output.teams.empty()) {
                output.teams[row] = state.team;
            }
            output.alive[row] = state.alive;
        }};
    auto const gather_group{
        [&](std::span<std::int32_t const> const rows, EntityType const type, auto&& read) {
            auto const indexes{indexes_.group(type)};
            auto const base{entity_identity_offsets[type]};
            EntityUniqueId previous;
            agent_accessor_detail::TargetState state{};
            for (auto const row : rows) {
                auto const id{ids[row]};
                if (id != previous) {
                    auto const offset{id.index()};
                    auto const index{offset >= base && offset - base < indexes.size()
                                         ? indexes[offset - base]
                                         : AgentIndexes::invalid_index};
                    state = index != AgentIndexes::invalid_index
                              ? read(static_cast<std::int32_t>(index))
                              : agent_accessor_detail::TargetState{};
                    if (!state.alive) {
                        state = {};
                    }
                    previous = id;
                }
                scatter(row, state);
            }
        }};

    std::size_t begin{};
    while (begin < count) {
        auto const type{ids[order[begin]].entity_type()};
        auto end{begin + 1};
        while (end < count && ids[order[end]].entity_type() == type) {
            ++end;
        }
        auto const group{order.subspan(begin, end - begin)};
        if (type >= EntityType::COUNT) {
            for (auto const row : group) {
                scatter(row, {});
            }
            begin = end;
            continue;
        }
        switch (type) {
            case EntityType::PlayerShip:
                gather_group(group, type, [&](std::int32_t) {
                    if (player_.transform == nullptr) {
                        return agent_accessor_detail::TargetState{};
                    }
                    return agent_accessor_detail::TargetState{
                        to_float(player_.transform->location),
                        to_float(*player_.velocity),
                        *player_.team,
                        sim::is_alive(health_table_.get_health(player_.health_index, player_.id))};
                });
                break;
            case EntityType::CapitalShip: {
                auto const healths{
                    health_table_.get_const_view(capitals_.health_indices, capitals_.entity_ids)};
                gather_group(group, type, [&](std::int32_t const index) {
                    return agent_accessor_detail::TargetState{capitals_.locations[index],
                                                              {},
                                                              capitals_.teams[index],
                                                              sim::is_alive(healths.health(index))};
                });
            } break;
            case EntityType::Fighter: {
                auto const healths{
                    health_table_.get_const_view(fighters_.health_indices, fighters_.entity_ids)};
                gather_group(group, type, [&](std::int32_t const index) {
                    return agent_accessor_detail::TargetState{fighters_.locations[index],
                                                              fighters_.velocities[index],
                                                              fighters_.teams[index],
                                                              sim::is_alive(healths.health(index))};
                });
            } break;
            case EntityType::Turret: {
                auto const healths{
                    health_table_.get_const_view(turrets_.health_indices, turrets_.entity_ids)};
                gather_group(group, type, [&](std::int32_t const index) {
                    return agent_accessor_detail::TargetState{turrets_.locations[index],
                                                              {},
                                                              turrets_.teams[index],
                                                              sim::is_alive(healths.health(index))};
                });
            } break;
            case EntityType::TubeSpinner:
                gather_group(group, type, [&](std::int32_t const index) {
                    return agent_accessor_detail::TargetState{
                        spinners_.locations[index], {}, Team::White, true};
                });
                break;
            case EntityType::COUNT:
                break;
        }
        begin = end;
    }
}
}

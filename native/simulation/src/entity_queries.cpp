#include <ioj/sim/entity_queries.h>

#include <ioj/sim/column_math.h>

#include <algorithm>
#include <cassert>
#include <numeric>

namespace ioj::sim {
namespace entity_query_detail {
struct TargetState {
    Vector3f location{};
    Vector3f velocity{};
    Team team{};
    bool alive{};
    Health health{};
    Rotator3f rotation{};
};
}

void gather_entities(EntityTables const& tables,
                     std::span<EntityUniqueId const> const ids,
                     std::span<std::uint32_t> const order,
                     EntityQueryView const output) {
    assert(tables.lookups.permits_lookup());
    auto const capitals_{tables.sources.capitals ? tables.sources.capitals->get_const_view()
                                                 : CapitalEntityData::ConstView{}};
    auto const fighters_{tables.sources.fighters ? tables.sources.fighters->get_const_view()
                                                 : FighterEntityData::ConstView{}};
    auto const turrets_{tables.sources.turrets ? tables.sources.turrets->get_const_view()
                                               : TurretEntityData::ConstView{}};
    auto const spinners_{tables.sources.spinners ? tables.sources.spinners->get_const_view()
                                                 : SpinnerEntityData::ConstView{}};
    auto const player_{tables.sources.player};
    auto const capitals_healths_{
        tables.health.get_const_view<EntityType::CapitalShip>(capitals_.num())};
    auto const fighters_healths_{
        tables.health.get_const_view<EntityType::Fighter>(fighters_.num())};
    auto const turrets_healths_{tables.health.get_const_view<EntityType::Turret>(turrets_.num())};
    auto const player_healths{tables.health.get_const_view<EntityType::PlayerShip>(
        player_.transform != nullptr ? 1u : 0u)};

    auto const read_locations{!output.locations.is_empty()};
    auto const read_velocities{!output.velocities.is_empty()};
    auto const read_teams{!output.teams.empty()};
    auto const read_health{!output.alive.empty() || !output.healths.empty()};
    auto const read_rotations{!output.rotations.empty()};
    auto const count{ids.size()};
    assert(order.size() == count && (output.alive.empty() || output.alive.size() == count));
    assert(output.locations.is_empty() ||
           static_cast<std::size_t>(output.locations.num()) == count);
    assert(output.velocities.is_empty() ||
           static_cast<std::size_t>(output.velocities.num()) == count);
    assert(output.teams.empty() || output.teams.size() == count);
    assert(output.healths.empty() || output.healths.size() == count);
    assert(output.rotations.empty() || output.rotations.size() == count);
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::sort(order, {}, [&](std::uint32_t const row) { return ids[row]; });
    auto const scatter{[&](std::uint32_t const row, entity_query_detail::TargetState const& state) {
        auto const spatial_alive{output.alive.empty() || state.alive};
        if (!output.locations.is_empty()) {
            output.locations.set(row, spatial_alive ? state.location : Vector3f{});
        }
        if (!output.healths.empty()) {
            output.healths[row] = state.health;
        }
        if (!output.rotations.empty()) {
            output.rotations[row] = spatial_alive ? state.rotation : Rotator3f{};
        }
        if (!output.velocities.is_empty()) {
            output.velocities.set(row, spatial_alive ? state.velocity : Vector3f{});
        }
        if (!output.teams.empty()) {
            output.teams[row] = state.team;
        }
        if (!output.alive.empty()) {
            output.alive[row] = state.alive;
        }
    }};
    auto const gather_group{
        [&](std::span<std::uint32_t const> const rows, EntityType const type, auto&& read) {
            auto const handles{tables.lookups.for_type(type).entries()};
            auto const handle_count{handles.size()};
            EntityUniqueId previous;
            entity_query_detail::TargetState state{};
            for (auto const row : rows) {
                auto const id{ids[row]};
                if (id != previous) {
                    auto const offset{id.index()};
                    auto const handle{offset < handle_count ? handles[offset]
                                                            : EntityInstanceHandle{}};
                    state = handle.is_valid() ? read(handle.index())
                                              : entity_query_detail::TargetState{};
                    previous = id;
                }
                scatter(row, state);
            }
        }};

    auto const capital_locations{capitals_.view_locations()};
    auto const capital_teams{capitals_.teams()};
    auto const fighter_locations{fighters_.view_locations()};
    auto const fighter_velocities{fighters_.view_velocities()};
    auto const fighter_teams{fighters_.teams()};
    auto const turret_locations{turrets_.view_locations()};
    auto const turret_teams{turrets_.teams()};
    auto const spinner_locations{spinners_.view_locations()};
    auto const capital_rotations{capitals_.view_rotations()};
    auto const turret_rotations{turrets_.view_rotations()};
    auto const fighter_directions{fighters_.view_aim_directions()};
    auto const spinner_yaws{spinners_.yaws()};
    std::size_t begin{};
    while (begin < count) {
        auto const type{ids[order[begin]].entity_type()};
        auto end{begin + 1};
        while (end < count && ids[order[end]].entity_type() == type) {
            ++end;
        }
        // Resolve each sorted type run against its own table.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const group{order.subspan(begin, end - begin)};
        if (std::to_underlying(type) >= ml::enum_count<EntityType>()) {
            for (auto const row : group) {
                scatter(row, {});
            }
            begin = end;
            continue;
        }
        switch (type) {
            case EntityType::PlayerShip:
                gather_group(group, type, [&](std::uint32_t) {
                    if (player_.transform == nullptr) {
                        return entity_query_detail::TargetState{};
                    }
                    return entity_query_detail::TargetState{
                        read_locations ? to_float(player_.transform->location) : Vector3f{},
                        read_velocities ? to_float(*player_.velocity) : Vector3f{},
                        read_teams ? *player_.team : Team{},
                        read_health && sim::is_alive(player_healths.health(0)),
                        read_health ? player_healths.health(0) : Health{},
                        read_rotations ? to_float(player_.transform->rotator()) : Rotator3f{}};
                });
                break;
            case EntityType::CapitalShip: {
                auto const healths{capitals_healths_};
                auto const locations{capital_locations};
                auto const teams{capital_teams};
                auto const rotations{capital_rotations};
                gather_group(group, type, [&](std::uint32_t const index) {
                    return entity_query_detail::TargetState{
                        read_locations ? locations[index] : Vector3f{},
                        {},
                        read_teams ? teams[index] : Team{},
                        read_health && sim::is_alive(healths.health(index)),
                        read_health ? healths.health(index) : Health{},
                        read_rotations ? rotation_at(rotations, index) : Rotator3f{}};
                });
            } break;
            case EntityType::Fighter: {
                auto const healths{fighters_healths_};
                auto const locations{fighter_locations};
                auto const velocities{fighter_velocities};
                auto const teams{fighter_teams};
                auto const directions{fighter_directions};
                gather_group(group, type, [&](std::uint32_t const index) {
                    return entity_query_detail::TargetState{
                        read_locations ? locations[index] : Vector3f{},
                        read_velocities ? velocities[index] : Vector3f{},
                        read_teams ? teams[index] : Team{},
                        read_health && sim::is_alive(healths.health(index)),
                        read_health ? healths.health(index) : Health{},
                        read_rotations ? direction_to_rotation(directions[index]) : Rotator3f{}};
                });
            } break;
            case EntityType::Turret: {
                auto const healths{turrets_healths_};
                auto const locations{turret_locations};
                auto const teams{turret_teams};
                auto const rotations{turret_rotations};
                gather_group(group, type, [&](std::uint32_t const index) {
                    return entity_query_detail::TargetState{
                        read_locations ? locations[index] : Vector3f{},
                        {},
                        read_teams ? teams[index] : Team{},
                        read_health && sim::is_alive(healths.health(index)),
                        read_health ? healths.health(index) : Health{},
                        read_rotations ? rotation_at(rotations, index) : Rotator3f{}};
                });
            } break;
            case EntityType::TubeSpinner: {
                auto const locations{spinner_locations};
                auto const yaws{spinner_yaws};
                gather_group(group, type, [&](std::uint32_t const index) {
                    return entity_query_detail::TargetState{
                        read_locations ? locations[index] : Vector3f{},
                        {},
                        Team::White,
                        true,
                        1000000,
                        read_rotations ? Rotator3f{.yaw = yaws[index]} : Rotator3f{}};
                });
            } break;
        }
        begin = end;
    }
}
}

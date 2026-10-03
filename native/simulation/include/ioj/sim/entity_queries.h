#pragma once
#include <ioj/sim/agent_display_batch.h>
#include <ioj/sim/column_math.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/frame_vectors3f.h>
#include <ioj/sim/rotator_math.h>

namespace ioj::sim {
struct EntityQueryView {
    Vectors3fView locations{};
    Vectors3fView velocities{};
    std::span<Team> teams{};
    std::span<std::uint8_t> alive{};
    std::span<Health> healths{};
    std::span<Rotator3f> rotations{};
};

// Gather only requested columns and scatter results back to caller order.
void gather_entities(EntityTables const& tables,
                     std::span<EntityUniqueId const> ids,
                     std::span<std::uint32_t> order,
                     EntityQueryView output);

inline auto entity_counts(EntityTables const& tables) -> EntityTypeSizes {
    EntityTypeSizes counts;
    counts[EntityType::PlayerShip] = tables.sources.player.transform != nullptr ? 1u : 0u;
    counts[EntityType::CapitalShip] = tables.sources.capitals ? tables.sources.capitals->num() : 0u;
    counts[EntityType::Fighter] = tables.sources.fighters ? tables.sources.fighters->num() : 0u;
    counts[EntityType::Turret] = tables.sources.turrets ? tables.sources.turrets->num() : 0u;
    counts[EntityType::TubeSpinner] = tables.sources.spinners ? tables.sources.spinners->num() : 0u;
    return counts;
}

inline auto display_batches(EntityTables const& tables) -> std::array<AgentDisplayBatch, 4> {
    auto const capitals_{tables.sources.capitals ? tables.sources.capitals->get_const_view()
                                                 : CapitalEntityData::ConstView{}};
    auto const fighters_{tables.sources.fighters ? tables.sources.fighters->get_const_view()
                                                 : FighterEntityData::ConstView{}};
    auto const turrets_{tables.sources.turrets ? tables.sources.turrets->get_const_view()
                                               : TurretEntityData::ConstView{}};
    auto const spinners_{tables.sources.spinners ? tables.sources.spinners->get_const_view()
                                                 : SpinnerEntityData::ConstView{}};
    auto const capitals_healths_{
        tables.health.get_const_view<EntityType::CapitalShip>(capitals_.num())};
    auto const fighters_healths_{
        tables.health.get_const_view<EntityType::Fighter>(fighters_.num())};
    auto const turrets_healths_{tables.health.get_const_view<EntityType::Turret>(turrets_.num())};
    return {{
        {EntityType::CapitalShip,
         capitals_.entity_ids(),
         capitals_.view_locations(),
         {},
         capitals_healths_,
         capitals_.teams()},
        {EntityType::Fighter,
         fighters_.entity_ids(),
         fighters_.view_locations(),
         fighters_.view_velocities(),
         fighters_healths_,
         fighters_.teams()},
        {EntityType::Turret,
         turrets_.entity_ids(),
         turrets_.view_locations(),
         {},
         turrets_healths_,
         turrets_.teams()},
        {EntityType::TubeSpinner, spinners_.entity_ids(), spinners_.view_locations(), {}, {}, {}},
    }};
}

template <typename Visitor>
void visit_live_entities(EntityTables const& tables, Visitor&& visit) {
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
    if (player_.transform != nullptr && sim::is_alive(player_healths.health(0))) {
        visit(player_.id,
              to_float(player_.transform->location),
              to_float(player_.transform->rotator()),
              *player_.team);
    }
    auto const capital_healths{capitals_healths_};
    auto const capital_ids{capitals_.entity_ids()};
    auto const capital_locations{capitals_.view_locations()};
    auto const capital_pitches{capitals_.view_rotations().pitches()};
    auto const capital_yaws{capitals_.view_rotations().yaws()};
    auto const capital_rolls{capitals_.view_rotations().rolls()};
    auto const capital_teams{capitals_.teams()};
    auto const capital_count{capitals_.num()};
    for (std::uint32_t i{}; i < capital_count; ++i) {
        if (sim::is_alive(capital_healths.health(i))) {
            visit(capital_ids[i],
                  capital_locations[i],
                  Rotator3f{capital_pitches[i], capital_yaws[i], capital_rolls[i]},
                  capital_teams[i]);
        }
    }
    auto const turret_healths{turrets_healths_};
    auto const turret_ids{turrets_.entity_ids()};
    auto const turret_locations{turrets_.view_locations()};
    auto const turret_pitches{turrets_.view_rotations().pitches()};
    auto const turret_yaws{turrets_.view_rotations().yaws()};
    auto const turret_rolls{turrets_.view_rotations().rolls()};
    auto const turret_teams{turrets_.teams()};
    auto const turret_count{turrets_.num()};
    for (std::uint32_t i{}; i < turret_count; ++i) {
        if (sim::is_alive(turret_healths.health(i))) {
            visit(turret_ids[i],
                  turret_locations[i],
                  Rotator3f{turret_pitches[i], turret_yaws[i], turret_rolls[i]},
                  turret_teams[i]);
        }
    }
    auto const fighter_healths{fighters_healths_};
    auto const fighter_ids{fighters_.entity_ids()};
    auto const fighter_locations{fighters_.view_locations()};
    auto const fighter_directions{fighters_.view_aim_directions()};
    auto const fighter_teams{fighters_.teams()};
    auto const fighter_count{fighters_.num()};
    for (std::uint32_t i{}; i < fighter_count; ++i) {
        if (sim::is_alive(fighter_healths.health(i))) {
            visit(fighter_ids[i],
                  fighter_locations[i],
                  direction_to_rotation(fighter_directions[i]),
                  fighter_teams[i]);
        }
    }
    auto const spinner_ids{spinners_.entity_ids()};
    auto const spinner_locations{spinners_.view_locations()};
    auto const spinner_yaws{spinners_.yaws()};
    auto const spinner_count{spinners_.num()};
    for (std::uint32_t i{}; i < spinner_count; ++i) {
        visit(spinner_ids[i],
              spinner_locations[i],
              Rotator3f{.pitch = 0.f, .yaw = spinner_yaws[i], .roll = 0.f},
              Team::White);
    }
}
}

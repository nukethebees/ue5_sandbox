#include <ioj/sim/level_sim.h>

#ifndef NDEBUG
#include <sandbox/core/diagnostics.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>

namespace ioj::sim {
/* **************************************** */
// Phase invariants
/* **************************************** */
auto LevelSim::check_phase_invariants(std::span<ThinkingEntityState const> const state) const
    -> bool {
    return check_thinking_entry_invariants(state) && check_thinking_phase_invariants(state);
}
auto LevelSim::capture_thinking_phase_state() const -> std::vector<ThinkingEntityState> {
    auto const capitals{capital_ships_simulation_.get_entities()};
    auto const fighters{fighters_simulation_.get_entities()};
    auto const turrets{turrets_simulation_.get_entities()};
    auto const spinners{spinners_simulation_.get_entities()};
    std::vector<ThinkingEntityState> state;
    state.reserve(static_cast<std::size_t>(capitals.num()) + fighters.num() + turrets.num() +
                  spinners.num() + player_ship_simulation_.has_value());

    auto const append_ships{[&]<EntityType Type>(auto const entities, auto orientation_at) {
        auto const ids{entities.entity_ids()};
        auto const healths{entity_tables_.health.get_const_view<Type>(entities.num())};
        auto const teams{entities.teams()};
        auto const locations{entities.view_locations()};
        auto const count{entities.num()};
        for (std::uint32_t row{}; row < count; ++row) {
            auto const id{ids[row]};
            assert(id.entity_type() == Type);
            auto const location{locations[row]};
            state.push_back({id,
                             row,
                             healths.health(row),
                             teams[row],
                             {location.X, location.Y, location.Z},
                             orientation_at(row)});
        }
    }};

    auto const capital_rotations{capitals.view_rotations()};
    append_ships.operator()<EntityType::CapitalShip>(
        capitals, [capital_rotations](std::uint32_t const row) {
            auto const [pitch, yaw, roll]{capital_rotations[row]};
            return std::array<double, 4>{pitch, yaw, roll, 0.0};
        });
    auto const fighter_directions{fighters.view_aim_directions()};
    append_ships.operator()<EntityType::Fighter>(
        fighters, [fighter_directions](std::uint32_t const row) {
            auto const direction{fighter_directions[row]};
            return std::array<double, 4>{direction.X, direction.Y, direction.Z, 0.0};
        });
    auto const turret_rotations{turrets.view_rotations()};
    append_ships.operator()<EntityType::Turret>(
        turrets, [turret_rotations](std::uint32_t const row) {
            auto const [pitch, yaw, roll]{turret_rotations[row]};
            return std::array<double, 4>{pitch, yaw, roll, 0.0};
        });

    auto const spinner_ids{spinners.entity_ids()};
    auto const spinner_locations{spinners.view_locations()};
    auto const spinner_yaws{spinners.yaws()};
    auto const spinner_count{spinners.num()};
    for (std::uint32_t row{}; row < spinner_count; ++row) {
        auto const id{spinner_ids[row]};
        assert(id.entity_type() == EntityType::TubeSpinner);
        auto const location{spinner_locations[row]};
        state.push_back({id,
                         row,
                         1,
                         Team::White,
                         {location.X, location.Y, location.Z},
                         {0.0, spinner_yaws[row], 0.0, 0.0}});
    }

    if (player_ship_simulation_) {
        auto const& player{*player_ship_simulation_};
        auto const id{player.unique_entity_id};
        assert(id.entity_type() == EntityType::PlayerShip);
        auto const& transform{player.get_physical_state().transform};
        auto const location{transform.location};
        auto const rotation{transform.rotation};
        state.push_back({id,
                         0,
                         player.get_health().health,
                         player.team,
                         {location.x, location.y, location.z},
                         {rotation.x, rotation.y, rotation.z, rotation.w}});
    }

    return state;
}
auto LevelSim::check_thinking_entry_invariants(
    std::span<ThinkingEntityState const> const state) const -> bool {
    if (clock_.phase() != SimulationPhase::Thinking) {
        return false;
    }

    // Grid membership is live at Thinking entry; retained owner rows need not all be alive.
    std::vector<EntityUniqueId> live_ids;
    live_ids.reserve(state.size());
    EntityTypeSizes maximum_healths;
    maximum_healths[EntityType::CapitalShip] = capital_ships_simulation_.config.max_health;
    maximum_healths[EntityType::Fighter] = fighters_simulation_.config.health;
    maximum_healths[EntityType::Turret] = turrets_simulation_.config.max_health;
    maximum_healths[EntityType::PlayerShip] =
        player_ship_simulation_ ? player_ship_simulation_->max_health_ : 1;
    maximum_healths[EntityType::TubeSpinner] = 1;
    std::array<std::span<EntityInstanceHandle const>, EntityTypeSizes::size()> handles;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        // Bind each type's table once before checking entities.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        handles[std::to_underlying(type)] = entity_tables_.lookups.for_type(type).entries();
    }

    for (auto const& entity : state) {
        if (std::to_underlying(entity.team) >= ml::enum_count<Team>() ||
            !std::ranges::all_of(entity.location,
                                 [](double const value) { return std::isfinite(value); }) ||
            !std::ranges::all_of(entity.orientation,
                                 [](double const value) { return std::isfinite(value); })) {
            ml::log_error(
                std::format("Invalid Thinking spatial state for entity {}", entity.id.raw_value()));
            return false;
        }
        auto const type{entity.id.entity_type()};
        // Select the already-bound span for this entity's type.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const table{handles[std::to_underlying(type)]};
        auto const handle{table[entity.id.index()]};
        if (is_alive(entity.health)) {
            if (!handle.is_valid() || handle.index() != entity.row ||
                handle.team() != entity.team ||
                handle.health_state() != quantise_health(entity.health, maximum_healths[type])) {
                ml::log_error(std::format("Invalid Thinking lookup snapshot for entity {}",
                                          entity.id.raw_value()));
                return false;
            }
            live_ids.push_back(entity.id);
        } else if (handle.is_valid()) {
            ml::log_error("Dead entity retained a published lookup handle");
            return false;
        }
    }

    auto const bounds{query_manager_.get_entity_collision_bounds()};
    auto const grid_ids{bounds.entity_ids()};
    std::vector<EntityUniqueId> indexed_ids{grid_ids.begin(), grid_ids.end()};
    std::ranges::sort(live_ids);
    std::ranges::sort(indexed_ids);
    if (live_ids != indexed_ids) {
        ml::log_error(
            "Thinking requires the spatial index to contain exactly the live spatial entities");
        return false;
    }

    return query_manager_.check_live_grid_membership();
}
auto LevelSim::check_thinking_phase_invariants(
    std::span<ThinkingEntityState const> const state) const -> bool {
    if (clock_.phase() != SimulationPhase::Thinking) {
        return false;
    }

    // Read owners afresh to catch direct SoA writes, including changes that bypass setters.
    auto const current{capture_thinking_phase_state()};
    if (current.size() != state.size()) {
        ml::log_error("Spatial entity membership changed during Thinking");
        return false;
    }
    auto const count{state.size()};
    for (std::size_t index{}; index < count; ++index) {
        if (current[index] != state[index]) {
            ml::log_error(std::format(
                "Thinking changed identity, row, health, team or transform for entity {}",
                state[index].id.raw_value()));
            return false;
        }
    }

    return true;
}
} // namespace ioj::sim
#endif

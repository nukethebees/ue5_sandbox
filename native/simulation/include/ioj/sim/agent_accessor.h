#pragma once

#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/health_table.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/transform3d.h>
#include <ioj/sim/turret_entity_data.h>

#include <array>
#include <ioj/sim/agent_display_batch.h>
#include <optional>

namespace ioj::sim {
struct AgentState {
    Vector3f location{};
    Vector3f velocity{};
    Rotator3f rotation{};
    Health health{};
    Team team{};
    bool damageable{true};

    [[nodiscard]] auto is_alive() const noexcept -> bool { return sim::is_alive(health); }
};

struct PlayerAgentView {
    Transform3d const* transform{};
    ml::Vector3d const* velocity{};
    HealthIndex health_index{};
    Team const* team{};
    EntityUniqueId id{};
};

struct AgentSpatialState {
    Vector3f location{};
    Health health{};
    Team team{};
};

struct AgentTargetView {
    Vectors3fView locations;
    Vectors3fView velocities;
    std::span<Team> teams;
    std::span<std::uint8_t> alive;
};

class AgentAccessor {
  public:
    AgentAccessor(AgentIndexes& indexes, HealthTable const& health_table) noexcept
        : indexes_{indexes}
        , health_table_{health_table} {}

    auto indexes() const noexcept -> AgentIndexes& { return indexes_; }

    [[nodiscard]] auto entity_counts() const noexcept -> EntityTypeSizes {
        EntityTypeSizes counts;
        counts[EntityType::PlayerShip] = player_.transform != nullptr ? 1u : 0u;
        counts[EntityType::CapitalShip] = static_cast<std::uint32_t>(capitals_.num());
        counts[EntityType::Fighter] = static_cast<std::uint32_t>(fighters_.num());
        counts[EntityType::Turret] = static_cast<std::uint32_t>(turrets_.num());
        counts[EntityType::TubeSpinner] = static_cast<std::uint32_t>(spinners_.num());
        return counts;
    }

    // The local player is presented separately, not as a world contact.
    auto display_batches() const -> std::array<AgentDisplayBatch, 4> {
        return {{
            {EntityType::CapitalShip,
             capitals_.entity_ids,
             capitals_.locations,
             {},
             health_table_.get_const_view(capitals_.health_indices),
             capitals_.teams},
            {EntityType::Fighter,
             fighters_.entity_ids,
             fighters_.locations,
             fighters_.velocities,
             health_table_.get_const_view(fighters_.health_indices),
             fighters_.teams},
            {EntityType::Turret,
             turrets_.entity_ids,
             turrets_.locations,
             {},
             health_table_.get_const_view(turrets_.health_indices),
             turrets_.teams},
            {EntityType::TubeSpinner, spinners_.entity_ids, spinners_.locations, {}, {}, {}},
        }};
    }

    // Visits owning rows directly; callbacks must not mutate storage.
    template <typename Visitor>
    void for_each_alive_spatial(Visitor&& visit) const {
        if (player_.transform != nullptr &&
            sim::is_alive(health_table_.get_health(player_.health_index))) {
            visit(player_.id,
                  to_float(player_.transform->location),
                  to_float(player_.transform->rotator()),
                  *player_.team);
        }
        auto const capital_healths{health_table_.get_const_view(capitals_.health_indices)};
        auto const capital_count{capitals_.num()};
        for (std::int32_t i{}; i < capital_count; ++i) {
            if (sim::is_alive(capital_healths.health(i))) {
                visit(capitals_.entity_ids[i],
                      capitals_.locations[i],
                      capitals_.rotations[i],
                      capitals_.teams[i]);
            }
        }
        auto const turret_healths{health_table_.get_const_view(turrets_.health_indices)};
        auto const turret_count{turrets_.num()};
        for (std::int32_t i{}; i < turret_count; ++i) {
            if (sim::is_alive(turret_healths.health(i))) {
                visit(turrets_.entity_ids[i],
                      turrets_.locations[i],
                      turrets_.rotations[i],
                      turrets_.teams[i]);
            }
        }
        auto const fighter_healths{health_table_.get_const_view(fighters_.health_indices)};
        auto const fighter_count{fighters_.num()};
        for (std::int32_t i{}; i < fighter_count; ++i) {
            if (sim::is_alive(fighter_healths.health(i))) {
                visit(fighters_.entity_ids[i],
                      fighters_.locations[i],
                      direction_to_rotation(fighters_.aim_directions[i]),
                      fighters_.teams[i]);
            }
        }
        auto const spinner_count{spinners_.num()};
        for (std::int32_t i{}; i < spinner_count; ++i) {
            visit(spinners_.entity_ids[i],
                  spinners_.locations[i],
                  Rotator3f{.pitch = 0.f, .yaw = spinners_.yaws[i], .roll = 0.f},
                  Team::White);
        }
    }

    // Sorts a scratch row permutation; IDs and destination SOA rows stay in caller order.
    // Velocities and teams may be omitted. Missing/dead targets produce zeroed fields.
    void gather_targets(std::span<EntityUniqueId const> ids,
                        std::span<std::int32_t> order,
                        AgentTargetView output) const;

    [[nodiscard]] auto read_spatial(EntityUniqueId const id) const
        -> std::optional<AgentSpatialState> {
        auto const index{indexes_.find(id)};
        if (index < 0) {
            return std::nullopt;
        }
        switch (id.entity_type()) {
            case EntityType::PlayerShip:
                if (player_.transform == nullptr) {
                    return std::nullopt;
                }
                return AgentSpatialState{to_float(player_.transform->location),
                                         health_table_.get_health(player_.health_index),
                                         *player_.team};
            case EntityType::CapitalShip:
                return AgentSpatialState{
                    capitals_.locations[index],
                    health_table_.get_const_view(capitals_.health_indices).health(index),
                    capitals_.teams[index]};
            case EntityType::Fighter:
                return AgentSpatialState{
                    fighters_.locations[index],
                    health_table_.get_const_view(fighters_.health_indices).health(index),
                    fighters_.teams[index]};
            case EntityType::Turret:
                return AgentSpatialState{
                    turrets_.locations[index],
                    health_table_.get_const_view(turrets_.health_indices).health(index),
                    turrets_.teams[index]};
            case EntityType::TubeSpinner:
                return AgentSpatialState{spinners_.locations[index], 1000000, Team::White};
            case EntityType::COUNT:
                return std::nullopt;
        }
        return std::nullopt;
    }

    [[nodiscard]] auto is_alive(EntityUniqueId const id) const noexcept -> bool {
        auto const index{indexes_.find(id)};
        if (index < 0) {
            return false;
        }
        switch (id.entity_type()) {
            case EntityType::PlayerShip:
                return player_.health_index.is_valid() &&
                       sim::is_alive(health_table_.get_health(player_.health_index));
            case EntityType::CapitalShip:
                return sim::is_alive(
                    health_table_.get_const_view(capitals_.health_indices).health(index));
            case EntityType::Fighter:
                return sim::is_alive(
                    health_table_.get_const_view(fighters_.health_indices).health(index));
            case EntityType::Turret:
                return sim::is_alive(
                    health_table_.get_const_view(turrets_.health_indices).health(index));
            case EntityType::TubeSpinner:
                return true;
            case EntityType::COUNT:
                return false;
        }
        return false;
    }

    void bind(CapitalEntityData::ConstView capitals,
              FighterEntityData::ConstView fighters,
              TurretEntityData::ConstView turrets,
              SpinnerEntityData::ConstView spinners,
              PlayerAgentView player = {}) noexcept {
        capitals_ = capitals;
        fighters_ = fighters;
        turrets_ = turrets;
        spinners_ = spinners;
        player_ = player;
    }

    [[nodiscard]] auto read(EntityUniqueId const id) const -> std::optional<AgentState> {
        auto const index{indexes_.find(id)};
        if (index < 0) {
            return std::nullopt;
        }

        switch (id.entity_type()) {
            case EntityType::PlayerShip:
                if (player_.transform == nullptr) {
                    return std::nullopt;
                }
                return AgentState{to_float(player_.transform->location),
                                  to_float(*player_.velocity),
                                  to_float(player_.transform->rotator()),
                                  health_table_.get_health(player_.health_index),
                                  *player_.team};
            case EntityType::CapitalShip:
                return AgentState{
                    capitals_.locations[index],
                    {},
                    capitals_.rotations[index],
                    health_table_.get_const_view(capitals_.health_indices).health(index),
                    capitals_.teams[index]};
            case EntityType::Fighter:
                return AgentState{
                    fighters_.locations[index],
                    fighters_.velocities[index],
                    direction_to_rotation(fighters_.aim_directions[index]),
                    health_table_.get_const_view(fighters_.health_indices).health(index),
                    fighters_.teams[index]};
            case EntityType::Turret:
                return AgentState{
                    turrets_.locations[index],
                    {},
                    turrets_.rotations[index],
                    health_table_.get_const_view(turrets_.health_indices).health(index),
                    turrets_.teams[index]};
            case EntityType::TubeSpinner:
                return AgentState{spinners_.locations[index],
                                  {},
                                  {.pitch = 0.f, .yaw = spinners_.yaws[index], .roll = 0.f},
                                  1000000,
                                  Team::White,
                                  false};
            case EntityType::COUNT:
                return std::nullopt;
        }
        return std::nullopt;
    }

    [[nodiscard]] auto read_alive(EntityUniqueId const id) const -> std::optional<AgentState> {
        auto result{read(id)};
        return result && result->is_alive() ? result : std::nullopt;
    }
  private:
    AgentIndexes& indexes_;
    HealthTable const& health_table_;
    CapitalEntityData::ConstView capitals_{};
    FighterEntityData::ConstView fighters_{};
    TurretEntityData::ConstView turrets_{};
    SpinnerEntityData::ConstView spinners_{};
    PlayerAgentView player_{};
};
}

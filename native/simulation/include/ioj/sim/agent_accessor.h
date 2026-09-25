#pragma once
#include <ioj/sim/column_math.h>

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
    // Bound entity and health views are borrowed. Do not query while their backing entity or
    // component storage is undergoing structural mutation; bind again after the commit.
    AgentAccessor(AgentIndices& indexes, HealthTable const& health_table) noexcept
        : indexes_{indexes}
        , health_table_{health_table} {}

    auto indexes() const noexcept -> AgentIndices& { return indexes_; }

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
            {EntityType::TubeSpinner,
             spinners_.entity_ids(),
             spinners_.view_locations(),
             {},
             {},
             {}},
        }};
    }

    // Visits owning rows directly; callbacks must not mutate storage.
    template <typename Visitor>
    void for_each_alive_spatial(Visitor&& visit) const {
        if (player_.transform != nullptr &&
            sim::is_alive(health_table_.get_health(player_.health_index, player_.id))) {
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
        for (std::int32_t i{}; i < capital_count; ++i) {
            if (sim::is_alive(capital_healths.health(i))) {
                visit(capital_ids[i],
                      vector_at(capital_locations, i),
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
        for (std::int32_t i{}; i < turret_count; ++i) {
            if (sim::is_alive(turret_healths.health(i))) {
                visit(turret_ids[i],
                      vector_at(turret_locations, i),
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
        for (std::int32_t i{}; i < fighter_count; ++i) {
            if (sim::is_alive(fighter_healths.health(i))) {
                visit(fighter_ids[i],
                      vector_at(fighter_locations, i),
                      direction_to_rotation(vector_at(fighter_directions, i)),
                      fighter_teams[i]);
            }
        }
        auto const spinner_ids{spinners_.entity_ids()};
        auto const spinner_locations{spinners_.view_locations()};
        auto const spinner_yaws{spinners_.yaws()};
        auto const spinner_count{spinners_.num()};
        for (std::int32_t i{}; i < spinner_count; ++i) {
            visit(spinner_ids[i],
                  vector_at(spinner_locations, i),
                  Rotator3f{.pitch = 0.f, .yaw = spinner_yaws[i], .roll = 0.f},
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
                                         health_table_.get_health(player_.health_index, player_.id),
                                         *player_.team};
            case EntityType::CapitalShip:
                return AgentSpatialState{vector_at(capitals_.view_locations(), index),
                                         capitals_healths_.health(index),
                                         capitals_.teams()[index]};
            case EntityType::Fighter:
                return AgentSpatialState{vector_at(fighters_.view_locations(), index),
                                         fighters_healths_.health(index),
                                         fighters_.teams()[index]};
            case EntityType::Turret:
                return AgentSpatialState{vector_at(turrets_.view_locations(), index),
                                         turrets_healths_.health(index),
                                         turrets_.teams()[index]};
            case EntityType::TubeSpinner:
                return AgentSpatialState{
                    vector_at(spinners_.view_locations(), index), 1000000, Team::White};
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
                return player_.transform != nullptr && player_.health_index.is_valid() &&
                       sim::is_alive(health_table_.get_health(player_.health_index, player_.id));
            case EntityType::CapitalShip:
                return sim::is_alive(capitals_healths_.health(index));
            case EntityType::Fighter:
                return sim::is_alive(fighters_healths_.health(index));
            case EntityType::Turret:
                return sim::is_alive(turrets_healths_.health(index));
            case EntityType::TubeSpinner:
                return true;
            case EntityType::COUNT:
                return false;
        }
        return false;
    }

    void bind(SingleAllocationCapitalEntityData::ConstView capitals,
              SingleAllocationFighterEntityData::ConstView fighters,
              SingleAllocationTurretEntityData::ConstView turrets,
              SingleAllocationSpinnerEntityData::ConstView spinners,
              PlayerAgentView player = {}) noexcept {
        capitals_ = capitals;
        fighters_ = fighters;
        turrets_ = turrets;
        spinners_ = spinners;
        player_ = player;
        capitals_healths_ =
            health_table_.get_const_view(capitals.health_indices(), capitals.entity_ids());
        fighters_healths_ =
            health_table_.get_const_view(fighters.health_indices(), fighters.entity_ids());
        turrets_healths_ =
            health_table_.get_const_view(turrets.health_indices(), turrets.entity_ids());
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
                                  health_table_.get_health(player_.health_index, player_.id),
                                  *player_.team};
            case EntityType::CapitalShip:
                return AgentState{vector_at(capitals_.view_locations(), index),
                                  {},
                                  rotation_at(capitals_.view_rotations(), index),
                                  capitals_healths_.health(index),
                                  capitals_.teams()[index]};
            case EntityType::Fighter:
                return AgentState{
                    vector_at(fighters_.view_locations(), index),
                    vector_at(fighters_.view_velocities(), index),
                    direction_to_rotation(vector_at(fighters_.view_aim_directions(), index)),
                    fighters_healths_.health(index),
                    fighters_.teams()[index]};
            case EntityType::Turret:
                return AgentState{vector_at(turrets_.view_locations(), index),
                                  {},
                                  rotation_at(turrets_.view_rotations(), index),
                                  turrets_healths_.health(index),
                                  turrets_.teams()[index]};
            case EntityType::TubeSpinner:
                return AgentState{vector_at(spinners_.view_locations(), index),
                                  {},
                                  {.pitch = 0.f, .yaw = spinners_.yaws()[index], .roll = 0.f},
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
    AgentIndices& indexes_;
    HealthTable const& health_table_;
    SingleAllocationCapitalEntityData::ConstView capitals_{};
    SingleAllocationFighterEntityData::ConstView fighters_{};
    SingleAllocationTurretEntityData::ConstView turrets_{};
    SingleAllocationSpinnerEntityData::ConstView spinners_{};
    PlayerAgentView player_{};
    HealthConstView capitals_healths_{};
    HealthConstView fighters_healths_{};
    HealthConstView turrets_healths_{};
};
}

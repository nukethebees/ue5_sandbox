#pragma once

#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/capital_entity_data.h>
#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/rotator_math.h>
#include <ioj/sim/spinner_entity_data.h>
#include <ioj/sim/transform3d.h>
#include <ioj/sim/turret_entity_data.h>

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
    Health const* health{};
    Team const* team{};
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
    explicit AgentAccessor(AgentIndexes& indexes) noexcept
        : indexes_{indexes} {}

    auto indexes() const noexcept -> AgentIndexes& { return indexes_; }

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
                return AgentSpatialState{
                    to_float(player_.transform->location), *player_.health, *player_.team};
            case EntityType::CapitalShip:
                return AgentSpatialState{
                    capitals_.locations[index], capitals_.healths[index], capitals_.teams[index]};
            case EntityType::Fighter:
                return AgentSpatialState{
                    fighters_.locations[index], fighters_.healths[index], fighters_.teams[index]};
            case EntityType::Turret:
                return AgentSpatialState{
                    turrets_.locations[index], turrets_.healths[index], turrets_.teams[index]};
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
                return player_.health != nullptr && sim::is_alive(*player_.health);
            case EntityType::CapitalShip:
                return sim::is_alive(capitals_.healths[index]);
            case EntityType::Fighter:
                return sim::is_alive(fighters_.healths[index]);
            case EntityType::Turret:
                return sim::is_alive(turrets_.healths[index]);
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
                                  *player_.health,
                                  *player_.team};
            case EntityType::CapitalShip:
                return AgentState{capitals_.locations[index],
                                  {},
                                  capitals_.rotations[index],
                                  capitals_.healths[index],
                                  capitals_.teams[index]};
            case EntityType::Fighter:
                return AgentState{fighters_.locations[index],
                                  fighters_.velocities[index],
                                  direction_to_rotation(fighters_.aim_directions[index]),
                                  fighters_.healths[index],
                                  fighters_.teams[index]};
            case EntityType::Turret:
                return AgentState{turrets_.locations[index],
                                  {},
                                  turrets_.rotations[index],
                                  turrets_.healths[index],
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
    CapitalEntityData::ConstView capitals_{};
    FighterEntityData::ConstView fighters_{};
    TurretEntityData::ConstView turrets_{};
    SpinnerEntityData::ConstView spinners_{};
    PlayerAgentView player_{};
};
}

#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/levels/level_definition_components.h>

#include <optional>
#include <utility>
#include <vector>

namespace ioj::levels {
// Own a validated definition. Construction verifies values and cross-component references.
class LevelDefinition {
  public:
    [[nodiscard]] static auto make(LevelDefinitionComponents components)
        -> std::expected<LevelDefinition, Diagnostics>;

    [[nodiscard]] auto metadata() const -> LevelMetadata const& { return metadata_; }
    [[nodiscard]] auto unlock_level_ids() const -> std::vector<LevelId> const& {
        return unlock_level_ids_;
    }
    [[nodiscard]] auto player_entity_id() const -> EntityId const& { return player_entity_id_; }
    [[nodiscard]] auto camera() const -> std::optional<LevelCameraDefinition> const& {
        return camera_;
    }
    [[nodiscard]] auto collision_grid() const
        -> std::optional<LevelCollisionGridDefinition> const& {
        return collision_grid_;
    }
    [[nodiscard]] auto mission() const -> std::optional<LevelMissionDefinition> const& {
        return mission_;
    }
    [[nodiscard]] auto mission_events() const -> std::vector<LevelMissionObjectiveEvent> const& {
        return mission_events_;
    }
    [[nodiscard]] auto entities() const -> std::vector<EntitySpawnDefinition> const& {
        return entities_;
    }
  private:
    explicit LevelDefinition(LevelDefinitionComponents components)
        : metadata_{std::move(components.metadata)}
        , unlock_level_ids_{std::move(components.unlock_level_ids)}
        , player_entity_id_{std::move(components.player_entity_id)}
        , camera_{std::move(components.camera)}
        , collision_grid_{std::move(components.collision_grid)}
        , mission_{std::move(components.mission)}
        , mission_events_{std::move(components.mission_events)}
        , entities_{std::move(components.entities)} {}

    LevelMetadata metadata_;
    std::vector<LevelId> unlock_level_ids_;
    EntityId player_entity_id_;
    std::optional<LevelCameraDefinition> camera_;
    std::optional<LevelCollisionGridDefinition> collision_grid_;
    std::optional<LevelMissionDefinition> mission_;
    std::vector<LevelMissionObjectiveEvent> mission_events_;
    std::vector<EntitySpawnDefinition> entities_;
};

// Return distinct teams from initial and scheduled entities, in enum order.
[[nodiscard]] auto participating_teams(LevelDefinition const& definition) -> std::vector<Team>;

} // namespace ioj::levels

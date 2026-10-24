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
    [[nodiscard]] static auto make(LevelDefinitionComponents definition)
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
    [[nodiscard]] auto mission() const -> std::optional<MissionDefinition> const& {
        return mission_;
    }
    [[nodiscard]] auto entities() const -> std::vector<EntitySpawnDefinition> const& {
        return entities_;
    }
    [[nodiscard]] auto settings() const -> settings::Settings const& { return settings_; }
    // Require an entity ID belonging to this definition.
    [[nodiscard]] auto entity(EntityId const& id) const -> EntitySpawnDefinition const&;
  private:
    explicit LevelDefinition(LevelDefinitionComponents components)
        : metadata_{std::move(components.metadata)}
        , unlock_level_ids_{std::move(components.unlock_level_ids)}
        , player_entity_id_{std::move(components.player_entity_id)}
        , camera_{std::move(components.camera)}
        , collision_grid_{std::move(components.collision_grid)}
        , mission_{std::move(components.mission)}
        , entities_{std::move(components.entities)}
        , settings_{std::move(components.settings)} {}

    LevelMetadata metadata_;
    std::vector<LevelId> unlock_level_ids_;
    EntityId player_entity_id_;
    std::optional<LevelCameraDefinition> camera_;
    std::optional<LevelCollisionGridDefinition> collision_grid_;
    std::optional<MissionDefinition> mission_;
    std::vector<EntitySpawnDefinition> entities_;
    settings::Settings settings_;
};

// Return distinct teams from initial and scheduled entities, in enum order.
[[nodiscard]] auto participating_teams(LevelDefinition const& definition) -> std::vector<Team>;

} // namespace ioj::levels

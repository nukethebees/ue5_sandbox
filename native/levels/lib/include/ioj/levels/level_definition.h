#pragma once

#include <ioj/levels/diagnostics.h>
#include <ioj/levels/entity_spawn_definition.h>
#include <ioj/levels/level_camera_definition.h>
#include <ioj/levels/level_collision_grid_definition.h>
#include <ioj/levels/level_metadata.h>
#include <ioj/levels/level_mission_definition.h>
#include <ioj/levels/level_mission_objective_event.h>

#include <optional>
#include <utility>
#include <vector>

namespace ioj::levels {
class LevelDefinition {
  public:
    // Assemble mutable components before construction. No LevelDefinition exists
    // until component values and cross-component references have passed validation.
    struct Components {
        LevelMetadata metadata{};
        std::vector<LevelId> unlock_level_ids{};
        EntityId player_entity_id{};
        std::optional<LevelCameraDefinition> camera{};
        std::optional<LevelCollisionGridDefinition> collision_grid{};
        std::optional<LevelMissionDefinition> mission{};
        std::vector<LevelMissionObjectiveEvent> mission_events{};
        std::vector<EntitySpawnDefinition> entities{};
    };

    [[nodiscard]] static auto make(Components components)
        -> std::expected<LevelDefinition, Diagnostics>;

    [[nodiscard]] auto components() const -> Components const& { return components_; }
  private:
    explicit LevelDefinition(Components components)
        : components_{std::move(components)} {}
    Components components_;
};

// Return distinct teams from initial and scheduled entities, in enum order.
[[nodiscard]] auto participating_teams(LevelDefinition const& definition) -> std::vector<Team>;

} // namespace ioj::levels

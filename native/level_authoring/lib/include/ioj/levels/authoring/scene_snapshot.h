#pragma once

#include <ioj/levels/level_definition.h>

namespace ioj::levels::authoring {
struct SceneEntityProperties {
    std::string label{};
    ml::Vector3d scale{1.0, 1.0, 1.0};
    bool class_matches{true};
};

// Ephemeral observation, not a validated level. Properties parallel definition.entities.
// The adapter checks actor validity/uniqueness; unresolved references remain diagnosable.
struct SceneSnapshot {
    LevelDefinition::Components definition{};
    std::vector<SceneEntityProperties> properties{};
    bool viewpoint_valid{true};
    bool mission_references_valid{true};
};

// Compare observations exactly, treating unchanged NaNs as equal so invalid scenes
// can be cached and repaired. Reference list order does not affect the scene.
[[nodiscard]] auto operator==(SceneSnapshot const& lhs, SceneSnapshot const& rhs) -> bool;
}

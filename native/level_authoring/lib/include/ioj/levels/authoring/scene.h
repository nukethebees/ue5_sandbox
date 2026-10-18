#pragma once

#include <ioj/levels/level_definition.h>

#include <cstddef>
#include <string>

namespace ioj::levels::authoring {
enum class SyncAction { Add, Update, Replace, Remove };

struct SceneEntityProperties {
    std::string label{};
    ml::Vector3d scale{1.0, 1.0, 1.0};
    bool class_matches{true};
};

// Ephemeral observation, not a validated level. Properties parallel definition.entities.
// The adapter checks actor validity/uniqueness; unresolved references remain diagnosable.
struct SceneSnapshot {
    LevelDefinition definition{};
    std::vector<SceneEntityProperties> properties{};
    bool viewpoint_valid{true};
    bool mission_references_valid{true};
};

struct SyncChange {
    EntityId id{};
    SyncAction action{};
    bool configure_actor{};
    bool relabel_actor{};
    // Index into the incoming definition; unused for Remove.
    std::size_t incoming_index{};
};

struct SyncPlan {
    std::vector<SyncChange> changes{};
    bool collision_grid_changed{};
    bool metadata_changed{};
    bool viewpoint_changed{};
    bool mission_changed{};

    [[nodiscard]] auto count(SyncAction action) const -> std::size_t;
    [[nodiscard]] auto has_changes() const -> bool;
};

// Validate the incoming editor subset, but allow invalid scene values to be overwritten.
[[nodiscard]] auto reconcile_scene(SceneSnapshot const& scene, LevelDefinition const& incoming)
    -> std::expected<SyncPlan, Diagnostics>;
// Compare observations without serializing or requiring semantic validity.
[[nodiscard]] auto same_scene(SceneSnapshot const& lhs, SceneSnapshot const& rhs) -> bool;
}

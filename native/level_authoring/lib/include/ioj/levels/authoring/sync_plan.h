#pragma once

#include <ioj/levels/identifiers.h>

#include <cstddef>
#include <vector>

namespace ioj::levels::authoring {
enum class SyncAction { Add, Update, Replace, Remove };

struct SyncChange {
    EntityId id{};
    SyncAction action{};
    bool configure_actor{};
    bool relabel_actor{};
    // Index into the incoming definition; unused for Remove.
    std::size_t incoming_index{};
};

struct SyncPlan {
    [[nodiscard]] auto count(SyncAction action) const -> std::size_t;
    [[nodiscard]] auto has_changes() const -> bool;

    std::vector<SyncChange> changes{};
    bool collision_grid_changed{};
    bool metadata_changed{};
    bool viewpoint_changed{};
    bool mission_changed{};
};
}

#pragma once

#include <ioj/levels/authoring/scene_snapshot.h>
#include <ioj/levels/authoring/sync_plan.h>

namespace ioj::levels::authoring {
// Compare a checked incoming level against a potentially invalid scene.
[[nodiscard]] auto reconcile_scene(SceneSnapshot const& scene, LevelDefinition const& level)
    -> SyncPlan;
}

#include "SandboxEditor/levels/S7LevelObserverCamera.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include <Engine/Level.h>

namespace ml::editor {
auto make_s7_observer_camera_transform(ULevel const& level,
                                       AS7LevelAuthoringDocument const& document)
    -> std::expected<TOptional<FObserverCameraTransform>, FString> {
    if (!document.use_observer_camera) {
        return TOptional<FObserverCameraTransform>{};
    }
    if (document.camera.targets.IsEmpty()) {
        return std::unexpected{TEXT("Observer camera requires at least one target.")};
    }

    TArray<FVector> target_positions;
    target_positions.Reserve(document.camera.targets.Num());
    for (auto const target : document.camera.targets) {
        if (!IsValid(target) || target->GetLevel() != &level) {
            return std::unexpected{
                TEXT("Every observer camera target must be a valid level actor.")};
        }
        auto const is_bound{document.entities.ContainsByPredicate(
            [target](FS7LevelEntityBinding const& binding) { return binding.actor == target; })};
        if (!is_bound) {
            return std::unexpected{TEXT("Every observer camera target must be an adopted entity.")};
        }
        target_positions.Add(target->GetActorLocation());
    }

    auto const transform{ml::make_observer_camera_transform(
        target_positions, document.camera.offset_direction, document.camera.distance)};
    if (!transform.IsSet()) {
        return std::unexpected{TEXT(
            "Observer camera needs finite targets, a non-zero direction, and positive distance.")};
    }
    return transform;
}
}

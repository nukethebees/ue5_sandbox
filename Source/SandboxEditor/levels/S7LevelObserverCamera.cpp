#include "SandboxEditor/levels/S7LevelObserverCamera.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include <ioj/presentation/observer_camera.h>

#include <SandboxCoreEngine/TransformConversion.h>

#include <Engine/Level.h>

#include <vector>

namespace ioj::editor {
auto make_s7_observer_camera_transform(ULevel const& level,
                                       AS7LevelAuthoringDocument const& document)
    -> std::expected<TOptional<FObserverCameraPreview>, FString> {
    if (!document.use_observer_camera) {
        return TOptional<FObserverCameraPreview>{};
    }
    if (document.camera.targets.IsEmpty()) {
        return std::unexpected{TEXT("Observer camera requires at least one target.")};
    }

    TSet<AActor const*> bound;
    for (auto const& binding : document.entities) {
        bound.Add(binding.actor);
    }
    std::vector<Vector3d> target_positions;
    target_positions.reserve(document.camera.targets.Num());
    for (auto const target : document.camera.targets) {
        if (!IsValid(target) || target->GetLevel() != &level) {
            return std::unexpected{
                TEXT("Every observer camera target must be a valid level actor.")};
        }
        if (!bound.Contains(target)) {
            return std::unexpected{TEXT("Every observer camera target must be an adopted entity.")};
        }
        target_positions.emplace_back(target->GetActorLocation());
    }

    auto const transform{presentation::make_observer_camera_transform(
        target_positions, Vector3d{document.camera.offset_direction}, document.camera.distance)};
    if (!transform) {
        return std::unexpected{TEXT(
            "Observer camera needs finite targets, a non-zero direction, and positive distance.")};
    }
    return TOptional<FObserverCameraPreview>{
        FObserverCameraPreview{to_unreal(transform->focus), to_unreal(transform->transform)}};
}
}

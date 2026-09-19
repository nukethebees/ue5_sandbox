#pragma once

#include <SpaceGame/levels/ObserverCameraTransform.h>

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ml::editor {
SANDBOXEDITOR_API auto make_s7_observer_camera_transform(ULevel const& level,
                                                         AS7LevelAuthoringDocument const& document)
    -> std::expected<TOptional<FObserverCameraTransform>, FString>;
}

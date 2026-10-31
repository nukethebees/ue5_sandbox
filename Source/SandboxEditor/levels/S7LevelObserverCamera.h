#pragma once

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
struct FObserverCameraPreview {
    FVector focus;
    FTransform transform;
};

SANDBOXEDITOR_API auto make_s7_observer_camera_transform(ULevel const& level,
                                                         AS7LevelAuthoringDocument const& document)
    -> std::expected<TOptional<FObserverCameraPreview>, FString>;
}

#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"

#include <CoreMinimal.h>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ioj::editor {
struct FObserverCameraPreview {
    FVector focus;
    FTransform transform;
};

SANDBOXEDITOR_API auto make_s7_observer_camera_transform(ULevel const& level,
                                                         AS7LevelAuthoringDocument const& document)
    -> S7AuthoringResult<TOptional<FObserverCameraPreview>>;
}

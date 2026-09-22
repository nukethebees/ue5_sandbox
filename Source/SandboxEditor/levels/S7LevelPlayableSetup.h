#pragma once

#include <CoreMinimal.h>

#include <expected>

class AS7LevelAuthoringDocument;
class ULevel;

namespace ml::editor {
SANDBOXEDITOR_API auto set_up_playable_s7_level(ULevel& level, AS7LevelAuthoringDocument& document)
    -> std::expected<FString, FString>;
}

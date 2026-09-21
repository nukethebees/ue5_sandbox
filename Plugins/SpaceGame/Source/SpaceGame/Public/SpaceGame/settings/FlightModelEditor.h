#pragma once

#include "CoreMinimal.h"

#include <ioj/sim/player/flight_model_config.h>

namespace ml::ioj {
enum class EFlightModelTranslationChannel : uint8 {
    Manual,
    Automatic,
};

SPACEGAME_API void apply_flight_model_translation_semantic_edit(
    ::ioj::sim::player::TranslationAxisConfig& axis,
    EFlightModelTranslationChannel channel,
    ::ioj::sim::player::TranslationSemantic semantic) noexcept;
}

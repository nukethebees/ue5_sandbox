#include "SpaceGame/settings/FlightModelEditor.h"

namespace ml::ioj {
void apply_flight_model_translation_semantic_edit(
    ::ioj::sim::player::TranslationAxisConfig& axis,
    EFlightModelTranslationChannel const channel,
    ::ioj::sim::player::TranslationSemantic const semantic) noexcept {
    using ::ioj::sim::player::TranslationInputSource;
    using ::ioj::sim::player::TranslationSemantic;

    auto const targets_velocity = [](TranslationSemantic const value) {
        return value == TranslationSemantic::TargetSpeed ||
               value == TranslationSemantic::TargetVelocity;
    };
    auto& selected{channel == EFlightModelTranslationChannel::Manual ? axis.manual
                                                                     : axis.automatic};
    auto& other{channel == EFlightModelTranslationChannel::Manual ? axis.automatic : axis.manual};

    if (targets_velocity(semantic) && targets_velocity(other.semantic)) {
        other.semantic = TranslationSemantic::Disabled;
    }
    selected.semantic = semantic;
    if (channel == EFlightModelTranslationChannel::Manual &&
        semantic == TranslationSemantic::TargetSpeed) {
        selected.input_source = TranslationInputSource::Axis;
    }
}
}

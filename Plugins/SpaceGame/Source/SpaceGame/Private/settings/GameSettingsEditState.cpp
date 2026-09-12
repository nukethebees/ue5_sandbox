#include "SpaceGame/settings/GameSettingsEditState.h"

namespace ml::ioj {
namespace {
auto setting_values_equal(FGameSettingValue const& left, FGameSettingValue const& right) -> bool {
    if (auto const* const left_float{std::get_if<float>(&left)}) {
        auto const* const right_float{std::get_if<float>(&right)};
        return right_float != nullptr &&
               (*left_float == *right_float ||
                (FMath::IsNaN(*left_float) && FMath::IsNaN(*right_float)));
    }
    return left == right;
}

auto graphics_preset(FGameSettingsState const& state) -> EGameGraphicsPreset {
    auto const quality{state.view_distance_quality};
    if (state.aa_quality != quality || state.shadow_quality != quality ||
        state.global_illumination_quality != quality || state.reflections_quality != quality ||
        state.post_processing_quality != quality || state.texture_quality != quality ||
        state.effects_quality != quality || state.shading_quality != quality) {
        return EGameGraphicsPreset::Custom;
    }

    return static_cast<EGameGraphicsPreset>(static_cast<uint8>(quality) + 1);
}

auto is_graphics_quality_setting(EGameSetting const setting) -> bool {
    switch (setting) {
        case EGameSetting::ViewDistanceQuality:
        case EGameSetting::AAQuality:
        case EGameSetting::ShadowQuality:
        case EGameSetting::GlobalIlluminationQuality:
        case EGameSetting::ReflectionsQuality:
        case EGameSetting::PostProcessingQuality:
        case EGameSetting::TextureQuality:
        case EGameSetting::EffectsQuality:
        case EGameSetting::ShadingQuality:
            return true;
        default:
            return false;
    }
}

void apply_graphics_preset(FGameSettingsState& state, EGameGraphicsPreset const preset) {
    if (preset == EGameGraphicsPreset::Custom) {
        return;
    }

    auto const quality{static_cast<EGameQualityLevel>(static_cast<uint8>(preset) - 1)};
    state.overall_quality = preset;
    state.view_distance_quality = quality;
    state.aa_quality = quality;
    state.shadow_quality = quality;
    state.global_illumination_quality = quality;
    state.reflections_quality = quality;
    state.post_processing_quality = quality;
    state.texture_quality = quality;
    state.effects_quality = quality;
    state.shading_quality = quality;
}
}

void FGameSettingsEditState::begin(FGameSettingsState applied, FGameSettingsState defaults) {
    applied.overall_quality = graphics_preset(applied);
    defaults.overall_quality = graphics_preset(defaults);
    applied_ = MoveTemp(applied);
    pending_ = applied_;
    defaults_ = MoveTemp(defaults);
}

void FGameSettingsEditState::cancel() {
    pending_ = applied_;
}

void FGameSettingsEditState::commit_all() {
    applied_ = pending_;
}

void FGameSettingsEditState::commit_setting(EGameSetting const setting) {
    auto const pending_value{game_setting_value(pending_, setting)};
    auto const set{set_game_setting_value(applied_, setting, pending_value)};
    ensureMsgf(set, TEXT("Could not commit game setting %d"), static_cast<int32>(setting));
}

auto FGameSettingsEditState::set_setting(EGameSetting const setting, FGameSettingValue const& value)
    -> bool {
    if (setting == EGameSetting::OverallQuality) {
        auto const* preset{std::get_if<EGameGraphicsPreset>(&value)};
        if (preset == nullptr) {
            return false;
        }
        apply_graphics_preset(pending_, *preset);
        return true;
    }

    if (!set_game_setting_value(pending_, setting, value)) {
        return false;
    }
    if (is_graphics_quality_setting(setting)) {
        pending_.overall_quality = graphics_preset(pending_);
    }
    return true;
}

void FGameSettingsEditState::reset_category(EGameSettingCategory const category) {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            static_cast<void>(set_game_setting_value(
                pending_, descriptor.id, game_setting_value(defaults_, descriptor.id)));
        }
    }
}

auto FGameSettingsEditState::applied() const -> FGameSettingsState const& {
    return applied_;
}

auto FGameSettingsEditState::pending() const -> FGameSettingsState const& {
    return pending_;
}

auto FGameSettingsEditState::defaults() const -> FGameSettingsState const& {
    return defaults_;
}

auto FGameSettingsEditState::value(EGameSetting const setting) const -> FGameSettingValue {
    return game_setting_value(pending_, setting);
}

auto FGameSettingsEditState::is_dirty() const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (!setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto FGameSettingsEditState::is_dirty(EGameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto FGameSettingsEditState::is_at_defaults(EGameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(defaults_, descriptor.id))) {
            return false;
        }
    }
    return true;
}

} // namespace ml::ioj

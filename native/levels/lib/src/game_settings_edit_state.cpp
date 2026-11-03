#include <ioj/settings/game_settings_edit_state.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <utility>

namespace ioj {
namespace {
auto setting_values_equal(GameSettingValue const& left, GameSettingValue const& right) -> bool {
    if (auto const* const left_float{std::get_if<float>(&left)}) {
        auto const* const right_float{std::get_if<float>(&right)};
        return right_float != nullptr && (*left_float == *right_float ||
                                          (std::isnan(*left_float) && std::isnan(*right_float)));
    }
    return left == right;
}

}

auto graphics_preset(GameSettingsState const& state) -> GameGraphicsPreset {
    auto const quality{state.view_distance_quality};
    if (state.aa_quality != quality || state.shadow_quality != quality ||
        state.global_illumination_quality != quality || state.reflections_quality != quality ||
        state.post_processing_quality != quality || state.texture_quality != quality ||
        state.effects_quality != quality || state.shading_quality != quality) {
        return GameGraphicsPreset::Custom;
    }

    return static_cast<GameGraphicsPreset>(static_cast<std::uint8_t>(quality) + 1);
}

auto game_anti_aliasing_method(std::int32_t const stored_value) -> GameAntiAliasingMethod {
    return static_cast<GameAntiAliasingMethod>(
        std::clamp(stored_value,
                   static_cast<std::int32_t>(GameAntiAliasingMethod::Off),
                   static_cast<std::int32_t>(GameAntiAliasingMethod::SMAA)));
}

auto game_audio_volume(float const value) -> float {
    auto const normalized{
        normalize_game_setting_value(game_setting_descriptor(GameSetting::MasterVolume), value)};
    return normalized ? std::get<float>(*normalized) : 1.f;
}

auto game_bees_count(std::int32_t const value) -> std::int32_t {
    auto const normalized{
        normalize_game_setting_value(game_setting_descriptor(GameSetting::Bees), value)};
    return std::get<std::int32_t>(*normalized);
}

auto game_setting_is_available(GameSettingAvailabilityProvider const provider,
                               GameSettingsState const& state) -> bool {
    return provider != GameSettingAvailabilityProvider::AAQuality ||
           state.aa_method != GameAntiAliasingMethod::Off;
}

auto normalize_game_setting_value(GameSettingDescriptor const& descriptor,
                                  GameSettingValue const& value,
                                  std::span<GameSettingValue const> const choices,
                                  bool const restrict_choices) -> std::optional<GameSettingValue> {
    GameSettingsState state;
    if (!set_game_setting_value(state, descriptor.id, value)) {
        return {};
    }
    if (auto const* number{std::get_if<float>(&value)};
        number != nullptr && !std::isfinite(*number)) {
        return {};
    }
    if (descriptor.control_kind == SettingControlKind::FloatRange) {
        return std::clamp(std::get<float>(value),
                          static_cast<float>(descriptor.minimum),
                          static_cast<float>(descriptor.maximum));
    }
    if (descriptor.control_kind == SettingControlKind::IntegerRange) {
        return std::clamp(std::get<std::int32_t>(value),
                          static_cast<std::int32_t>(descriptor.minimum),
                          static_cast<std::int32_t>(descriptor.maximum));
    }
    if (descriptor.control_kind == SettingControlKind::Choice && restrict_choices &&
        std::ranges::find(choices, value) == choices.end()) {
        return {};
    }
    if (descriptor.control_kind == SettingControlKind::Choice && !restrict_choices &&
        descriptor.options_provider != GameSettingOptionProvider::SupportedResolutions &&
        descriptor.options_provider != GameSettingOptionProvider::None) {
        auto const available{game_setting_choices(descriptor.options_provider)};
        if (std::ranges::find(available, value, &GameSettingChoice::value) == available.end()) {
            return {};
        }
    }
    return value;
}

auto game_setting_choices(GameSettingOptionProvider const provider,
                          std::span<ScreenResolution const> const resolutions)
    -> std::vector<GameSettingChoice> {
    switch (provider) {
        case GameSettingOptionProvider::SupportedResolutions: {
            std::vector<ScreenResolution> sorted{resolutions.begin(), resolutions.end()};
            std::erase_if(sorted,
                          [](auto const value) { return value.width <= 0 || value.height <= 0; });
            std::ranges::sort(sorted, [](auto const left, auto const right) {
                return left.width == right.width ? left.height < right.height
                                                 : left.width < right.width;
            });
            auto const duplicates{std::ranges::unique(sorted)};
            sorted.erase(duplicates.begin(), duplicates.end());
            std::vector<GameSettingChoice> result;
            result.reserve(sorted.size());
            for (auto const resolution : sorted) {
                result.push_back(
                    {resolution, std::format("{} x {}", resolution.width, resolution.height)});
            }
            return result;
        }
        case GameSettingOptionProvider::WindowModes:
            return {{GameWindowMode::Windowed, "Windowed"},
                    {GameWindowMode::Borderless, "Borderless"},
                    {GameWindowMode::Fullscreen, "Fullscreen"}};
        case GameSettingOptionProvider::FrameRateLimits: {
            std::vector<GameSettingChoice> result{{0.f, "Uncapped"}};
            for (auto const limit : {30, 60, 90, 120, 144, 165, 240}) {
                result.push_back({static_cast<float>(limit), std::format("{}", limit)});
            }
            return result;
        }
        case GameSettingOptionProvider::AAMethods:
            return {{GameAntiAliasingMethod::Off, "Off"},
                    {GameAntiAliasingMethod::FXAA, "FXAA"},
                    {GameAntiAliasingMethod::TAA, "TAA"},
                    {GameAntiAliasingMethod::TSR, "TSR"},
                    {GameAntiAliasingMethod::SMAA, "SMAA"}};
        case GameSettingOptionProvider::GraphicsPresets:
            return {{GameGraphicsPreset::Custom, "Custom"},
                    {GameGraphicsPreset::Low, "Low"},
                    {GameGraphicsPreset::Medium, "Medium"},
                    {GameGraphicsPreset::High, "High"},
                    {GameGraphicsPreset::Epic, "Epic"}};
        case GameSettingOptionProvider::QualityLevels:
            return {{GameQualityLevel::Low, "Low"},
                    {GameQualityLevel::Medium, "Medium"},
                    {GameQualityLevel::High, "High"},
                    {GameQualityLevel::Epic, "Epic"}};
        case GameSettingOptionProvider::None:
            return {};
    }
    assert(false && "Invalid settings option provider");
    std::unreachable();
}

namespace {
auto is_graphics_quality_setting(GameSetting const setting) -> bool {
    switch (setting) {
        case GameSetting::ViewDistanceQuality:
        case GameSetting::AAQuality:
        case GameSetting::ShadowQuality:
        case GameSetting::GlobalIlluminationQuality:
        case GameSetting::ReflectionsQuality:
        case GameSetting::PostProcessingQuality:
        case GameSetting::TextureQuality:
        case GameSetting::EffectsQuality:
        case GameSetting::ShadingQuality:
            return true;
        default:
            return false;
    }
}

void apply_graphics_preset(GameSettingsState& state, GameGraphicsPreset const preset) {
    if (preset == GameGraphicsPreset::Custom) {
        return;
    }

    auto const quality{static_cast<GameQualityLevel>(static_cast<std::uint8_t>(preset) - 1)};
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

void GameSettingsEditState::begin(GameSettingsState applied, GameSettingsState defaults) {
    applied.overall_quality = graphics_preset(applied);
    defaults.overall_quality = graphics_preset(defaults);
    applied_ = applied;
    pending_ = applied_;
    defaults_ = defaults;
}

void GameSettingsEditState::cancel() {
    pending_ = applied_;
}

void GameSettingsEditState::commit_all() {
    applied_ = pending_;
}

void GameSettingsEditState::commit_setting(GameSetting const setting) {
    auto const pending_value{game_setting_value(pending_, setting)};
    auto const set{set_game_setting_value(applied_, setting, pending_value)};
    assert(set);
    static_cast<void>(set);
}

auto GameSettingsEditState::set_setting(GameSetting const setting, GameSettingValue const& value)
    -> bool {
    auto const normalized{normalize_game_setting_value(game_setting_descriptor(setting), value)};
    if (!normalized) {
        return false;
    }
    if (setting == GameSetting::OverallQuality) {
        auto const* preset{std::get_if<GameGraphicsPreset>(&value)};
        if (preset == nullptr) {
            return false;
        }
        apply_graphics_preset(pending_, *preset);
        return true;
    }

    if (!set_game_setting_value(pending_, setting, *normalized)) {
        return false;
    }
    if (is_graphics_quality_setting(setting)) {
        pending_.overall_quality = graphics_preset(pending_);
    }
    return true;
}

void GameSettingsEditState::reset_category(GameSettingCategory const category) {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            static_cast<void>(set_game_setting_value(
                pending_, descriptor.id, game_setting_value(defaults_, descriptor.id)));
        }
    }
}

auto GameSettingsEditState::applied() const -> GameSettingsState const& {
    return applied_;
}

auto GameSettingsEditState::pending() const -> GameSettingsState const& {
    return pending_;
}

auto GameSettingsEditState::defaults() const -> GameSettingsState const& {
    return defaults_;
}

auto GameSettingsEditState::value(GameSetting const setting) const -> GameSettingValue {
    return game_setting_value(pending_, setting);
}

auto GameSettingsEditState::is_dirty() const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (!setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto GameSettingsEditState::is_dirty(GameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto GameSettingsEditState::is_at_defaults(GameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(defaults_, descriptor.id))) {
            return false;
        }
    }
    return true;
}

} // namespace ioj

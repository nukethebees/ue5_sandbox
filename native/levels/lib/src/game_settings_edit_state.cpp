#include <ioj/settings/game_settings_edit_state.h>

#include <ioj/are_equal.h>
#include <ioj/enum_strings.h>
#include <ioj/enum_traits.h>
#include <ioj/input/logical/flight_controls.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

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

template <typename Enum>
auto enum_choices() -> std::vector<GameSettingChoice> {
    auto const& values{EnumTraits<Enum>::values};
    std::vector<GameSettingChoice> choices;
    choices.reserve(values.size());
    for (auto const value : values) {
        choices.push_back({value, std::string{to_display_string(value)}});
    }

    return choices;
}
}

auto graphics_preset(GameSettingsState const& state) -> GameGraphicsPreset {
    auto const& video{state.video};
    auto const quality{video.view_distance_quality};
    if (!are_equal(quality,
                   video.aa_quality,
                   video.shadow_quality,
                   video.global_illumination_quality,
                   video.reflections_quality,
                   video.post_processing_quality,
                   video.texture_quality,
                   video.effects_quality,
                   video.shading_quality)) {
        return GameGraphicsPreset::Custom;
    }

    return static_cast<GameGraphicsPreset>(static_cast<std::uint8_t>(quality) + 1);
}

auto game_audio_volume(float const value) -> float {
    auto const normalized{
        normalize_game_setting_value(game_setting_descriptor(GameSetting::MasterVolume), value)};
    return normalized ? std::get<float>(*normalized) : 1.f;
}

auto game_setting_is_available(GameSettingAvailabilityProvider const provider,
                               GameSettingsState const& state) -> bool {
    return provider != GameSettingAvailabilityProvider::AAQuality ||
           state.video.aa_method != GameAntiAliasingMethod::Off;
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
            return enum_choices<GameWindowMode>();
        case GameSettingOptionProvider::FrameRateLimits: {
            std::vector<GameSettingChoice> result{{0.f, "Uncapped"}};
            for (auto const limit : {30, 60, 90, 120, 144, 165, 240}) {
                result.push_back({static_cast<float>(limit), std::format("{}", limit)});
            }

            return result;
        }
        case GameSettingOptionProvider::AAMethods:
            return enum_choices<GameAntiAliasingMethod>();
        case GameSettingOptionProvider::GraphicsPresets:
            return enum_choices<GameGraphicsPreset>();
        case GameSettingOptionProvider::QualityLevels:
            return enum_choices<GameQualityLevel>();
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
    state.video.overall_quality = preset;
    state.video.view_distance_quality = quality;
    state.video.aa_quality = quality;
    state.video.shadow_quality = quality;
    state.video.global_illumination_quality = quality;
    state.video.reflections_quality = quality;
    state.video.post_processing_quality = quality;
    state.video.texture_quality = quality;
    state.video.effects_quality = quality;
    state.video.shading_quality = quality;
}
}

void GameSettingsEditState::begin(GameSettingsState applied, GameSettingsState defaults) {
    binding_edit_ = {};
    applied.video.overall_quality = graphics_preset(applied);
    defaults.video.overall_quality = graphics_preset(defaults);
    applied_ = applied;
    pending_ = applied_;
    defaults_ = defaults;
}

void GameSettingsEditState::cancel() {
    binding_edit_ = {};
    pending_ = applied_;
}

void GameSettingsEditState::commit_all() {
    applied_ = pending_;
}

auto GameSettingsEditState::set_setting(GameSetting const setting, GameSettingValue const& value)
    -> bool {
    auto const normalized{normalize_game_setting_value(game_setting_descriptor(setting), value)};
    if (!normalized) {
        return false;
    }

    if (setting == GameSetting::OverallQuality) {
        apply_graphics_preset(pending_, std::get<GameGraphicsPreset>(*normalized));
        return true;
    }

    static_cast<void>(set_game_setting_value(pending_, setting, *normalized));
    if (is_graphics_quality_setting(setting)) {
        pending_.video.overall_quality = graphics_preset(pending_);
    }

    return true;
}

auto GameSettingsEditState::binding_conflicts(FlightMapping mapping, PhysicalKey key) const
    -> std::vector<FlightMapping> {
    std::vector<FlightMapping> conflicts;
    if (key == PhysicalKey::Unbound) {
        return conflicts;
    }
    for (auto other : EnumTraits<FlightMapping>::values) {
        if (other != mapping && is_gamepad_mapping(other) == is_gamepad_mapping(mapping) &&
            pending_.controls.bindings[other] == key) {
            conflicts.push_back(other);
        }
    }
    return conflicts;
}

auto GameSettingsEditState::set_binding(FlightMapping mapping,
                                        PhysicalKey key,
                                        bool replace_conflicts) -> bool {
    if (key != PhysicalKey::Unbound && !accepts_binding(mapping, key)) {
        return false;
    }
    auto const conflicts{binding_conflicts(mapping, key)};
    if (!conflicts.empty() && !replace_conflicts) {
        return false;
    }
    for (auto other : conflicts) {
        pending_.controls.bindings[other] = PhysicalKey::Unbound;
    }
    pending_.controls.bindings[mapping] = key;
    return true;
}

void GameSettingsEditState::reset_category(GameSettingCategory const category) {
    if (category == GameSettingCategory::Controls) {
        pending_.controls.bindings = defaults_.controls.bindings;
    }

    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            static_cast<void>(set_game_setting_value(
                pending_, descriptor.id, game_setting_value(defaults_, descriptor.id)));
        }
    }
}

void GameSettingsEditState::manage_binding(FlightMapping mapping) {
    binding_edit_ = {.mapping = mapping, .prompt = BindingPrompt::Manage};
}

auto GameSettingsEditState::binding_action_available(BindingChoice choice) const -> bool {
    auto const mapping{binding_edit_.mapping};
    switch (choice) {
        case BindingChoice::Change:
            return binding_edit_.prompt == BindingPrompt::Manage;
        case BindingChoice::Clear:
            return binding_edit_.prompt == BindingPrompt::Manage &&
                   pending_.controls.bindings[mapping] != PhysicalKey::Unbound;
        case BindingChoice::Reset:
            return binding_edit_.prompt == BindingPrompt::Manage &&
                   pending_.controls.bindings[mapping] != defaults_.controls.bindings[mapping];
        case BindingChoice::Replace:
            return binding_edit_.prompt == BindingPrompt::Conflict;
        case BindingChoice::Cancel:
            return binding_edit_.prompt != BindingPrompt::Closed;
    }
    std::unreachable();
}

void GameSettingsEditState::choose_binding_action(BindingChoice choice) {
    if (!binding_action_available(choice)) {
        return;
    }

    switch (choice) {
        case BindingChoice::Change:
            binding_edit_.prompt = BindingPrompt::Capture;
            return;
        case BindingChoice::Reset:
            propose_binding(defaults_.controls.bindings[binding_edit_.mapping]);
            return;
        case BindingChoice::Clear:
            propose_binding(PhysicalKey::Unbound);
            return;
        case BindingChoice::Replace:
            set_binding(binding_edit_.mapping, binding_edit_.candidate, true);
            break;
        case BindingChoice::Cancel:
            break;
    }
    binding_edit_.prompt = BindingPrompt::Closed;
}

void GameSettingsEditState::capture_binding(PhysicalKey key) {
    if (key == PhysicalKey::Escape || key == PhysicalKey::GamepadFaceButtonRight) {
        choose_binding_action(BindingChoice::Cancel);
    } else {
        propose_binding(key);
    }
}

void GameSettingsEditState::propose_binding(PhysicalKey key) {
    auto& edit{binding_edit_};
    edit.candidate = key;
    if (key != PhysicalKey::Unbound && !accepts_binding(edit.mapping, key)) {
        edit.prompt = BindingPrompt::Unsupported;
        return;
    }
    edit.conflicts = binding_conflicts(edit.mapping, key);
    if (!edit.conflicts.empty()) {
        edit.prompt = BindingPrompt::Conflict;
        edit.selected = BindingChoice::Replace;
        return;
    }
    set_binding(edit.mapping, key, false);
    edit.prompt = BindingPrompt::Closed;
}

void GameSettingsEditState::navigate_binding(MenuCommand command, bool repeat) {
    if (command == MenuCommand::Accept || command == MenuCommand::Back) {
        if (!repeat) {
            choose_binding_action(command == MenuCommand::Back ? BindingChoice::Cancel
                                                               : binding_edit_.selected);
        }
        return;
    }
    constexpr std::array choices{BindingChoice::Change,
                                 BindingChoice::Clear,
                                 BindingChoice::Reset,
                                 BindingChoice::Replace,
                                 BindingChoice::Cancel};
    auto index{static_cast<std::size_t>(binding_edit_.selected)};
    auto const backward{command == MenuCommand::Up || command == MenuCommand::Left ||
                        command == MenuCommand::Previous};
    do {
        index = (index + (backward ? choices.size() - 1 : 1)) % choices.size();
    } while (!binding_action_available(choices[index]));
    binding_edit_.selected = choices[index];
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
    if (pending_.controls.bindings != applied_.controls.bindings) {
        return true;
    }

    for (auto const& descriptor : game_setting_descriptors()) {
        if (!setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }

    return false;
}

auto GameSettingsEditState::is_dirty(GameSettingCategory const category) const -> bool {
    if (category == GameSettingCategory::Controls &&
        pending_.controls.bindings != applied_.controls.bindings) {
        return true;
    }

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
    if (category == GameSettingCategory::Controls &&
        pending_.controls.bindings != defaults_.controls.bindings) {
        return false;
    }

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

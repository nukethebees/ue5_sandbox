#include "SpaceGame/settings/GameSettingsBackend.h"

#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ioj {
namespace {

auto to_game_window_mode(EWindowMode::Type const value) -> EGameWindowMode {
    switch (value) {
        case EWindowMode::Fullscreen: {
            return EGameWindowMode::Fullscreen;
        }
        case EWindowMode::WindowedFullscreen: {
            return EGameWindowMode::Borderless;
        }
        default: {
            return EGameWindowMode::Windowed;
        }
    }
}

auto to_engine_window_mode(EGameWindowMode const value) -> EWindowMode::Type {
    switch (value) {
        case EGameWindowMode::Fullscreen: {
            return EWindowMode::Fullscreen;
        }
        case EGameWindowMode::Borderless: {
            return EWindowMode::WindowedFullscreen;
        }
        default: {
            return EWindowMode::Windowed;
        }
    }
}

auto to_quality(int32 const value) -> EGameQualityLevel {
    return static_cast<EGameQualityLevel>(FMath::Clamp(value, 0, 3));
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

auto make_option(FGameSettingValue value, TCHAR const* label) -> FGameSettingOption {
    return FGameSettingOption{MoveTemp(value), FText::FromString(label)};
}

auto read_settings(USpaceGameUserSettings& settings,
                   USpaceGameInputUserSettings const& input,
                   FIntPoint const resolution) -> FGameSettingsState {
    float normalized_scale{};
    float resolution_scale{};
    float minimum_scale{};
    float maximum_scale{};
    settings.GetResolutionScaleInformationEx(
        normalized_scale, resolution_scale, minimum_scale, maximum_scale);

    FGameSettingsState result{
        .resolution = resolution,
        .window_mode = to_game_window_mode(settings.GetFullscreenMode()),
        .vsync = settings.IsVSyncEnabled(),
        .frame_rate_limit = settings.GetFrameRateLimit(),
        .resolution_scale = resolution_scale,
        .view_distance_quality = to_quality(settings.GetViewDistanceQuality()),
        .aa_method = settings.anti_aliasing_method(),
        .aa_quality = to_quality(settings.GetAntiAliasingQuality()),
        .shadow_quality = to_quality(settings.GetShadowQuality()),
        .global_illumination_quality = to_quality(settings.GetGlobalIlluminationQuality()),
        .reflections_quality = to_quality(settings.GetReflectionQuality()),
        .post_processing_quality = to_quality(settings.GetPostProcessingQuality()),
        .texture_quality = to_quality(settings.GetTextureQuality()),
        .effects_quality = to_quality(settings.GetVisualEffectQuality()),
        .shading_quality = to_quality(settings.GetShadingQuality()),
        .bloom = settings.bloom_enabled(),
        .motion_blur = settings.motion_blur_enabled(),
        .master_volume = settings.master_volume(),
        .music_volume = settings.music_volume(),
        .sfx_volume = settings.sfx_volume(),
        .ui_volume = settings.ui_volume(),
        .bees = settings.bees(),
        .mouse_turn_sensitivity = input.mouse_turn_sensitivity(),
        .gamepad_turn_sensitivity = input.gamepad_turn_sensitivity(),
        .gamepad_turn_dead_zone = input.gamepad_turn_dead_zone(),
        .gamepad_move_dead_zone = input.gamepad_move_dead_zone(),
        .invert_mouse_pitch = input.invert_mouse_pitch(),
        .invert_mouse_yaw = input.invert_mouse_yaw(),
        .invert_gamepad_pitch = input.invert_gamepad_pitch(),
        .invert_gamepad_yaw = input.invert_gamepad_yaw(),
        .invert_gamepad_roll = input.invert_gamepad_roll(),
        .invert_gamepad_vertical_translation = input.invert_gamepad_vertical_translation(),
    };
    result.overall_quality = graphics_preset(result);
    return result;
}

} // namespace

void FGameSettingsBackend::set_local_player(ULocalPlayer* const local_player) {
    local_player_ = local_player;
}

auto FGameSettingsBackend::read() const -> FGameSettingsState {
    auto* const settings{user_settings()};
    if (settings == nullptr) {
        return {};
    }
    auto const* input{input_user_settings()};
    if (input == nullptr) {
        input = GetDefault<USpaceGameInputUserSettings>();
    }
    return read_settings(*settings, *input, settings->GetScreenResolution());
}

auto FGameSettingsBackend::defaults() const -> FGameSettingsState {
    auto* defaults{NewObject<USpaceGameUserSettings>()};
    defaults->SetToDefaults();

    auto* current{user_settings()};
    auto resolution{current != nullptr ? current->GetDesktopResolution() : FIntPoint{1920, 1080}};
    if (resolution.X <= 0 || resolution.Y <= 0) {
        resolution = FIntPoint{1920, 1080};
    }

    return read_settings(*defaults, *GetDefault<USpaceGameInputUserSettings>(), resolution);
}

void FGameSettingsBackend::preview_immediate(FGameSettingsState const& state,
                                             EGameSetting const setting) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    switch (setting) {
        case EGameSetting::MasterVolume: {
            settings->set_master_volume(state.master_volume);
            break;
        }
        case EGameSetting::MusicVolume: {
            settings->set_music_volume(state.music_volume);
            break;
        }
        case EGameSetting::SfxVolume: {
            settings->set_sfx_volume(state.sfx_volume);
            break;
        }
        case EGameSetting::UIVolume: {
            settings->set_ui_volume(state.ui_volume);
            break;
        }
        case EGameSetting::Bees: {
            settings->set_bees(state.bees);
            break;
        }
        case EGameSetting::MouseTurnSensitivity:
        case EGameSetting::GamepadTurnSensitivity:
        case EGameSetting::GamepadTurnDeadZone:
        case EGameSetting::GamepadMoveDeadZone:
        case EGameSetting::InvertMousePitch:
        case EGameSetting::InvertMouseYaw:
        case EGameSetting::InvertGamepadPitch:
        case EGameSetting::InvertGamepadYaw:
        case EGameSetting::InvertGamepadRoll:
        case EGameSetting::InvertGamepadVerticalTranslation: {
            apply_input_settings(state);
            break;
        }
        default: {
            break;
        }
    }
}

void FGameSettingsBackend::apply_non_display(FGameSettingsState const& state) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    write_non_display(*settings, state);
    settings->ApplyNonResolutionSettings();
    apply_input_settings(state);
}

void FGameSettingsBackend::apply_input_settings(FGameSettingsState const& state) const {
    if (auto* const input_settings{input_user_settings()}) {
        input_settings->set_mouse_turn_sensitivity(state.mouse_turn_sensitivity);
        input_settings->set_gamepad_turn_sensitivity(state.gamepad_turn_sensitivity);
        input_settings->set_gamepad_turn_dead_zone(state.gamepad_turn_dead_zone);
        input_settings->set_gamepad_move_dead_zone(state.gamepad_move_dead_zone);
        input_settings->set_invert_mouse_pitch(state.invert_mouse_pitch);
        input_settings->set_invert_mouse_yaw(state.invert_mouse_yaw);
        input_settings->set_invert_gamepad_pitch(state.invert_gamepad_pitch);
        input_settings->set_invert_gamepad_yaw(state.invert_gamepad_yaw);
        input_settings->set_invert_gamepad_roll(state.invert_gamepad_roll);
        input_settings->set_invert_gamepad_vertical_translation(
            state.invert_gamepad_vertical_translation);
        input_settings->ApplySettings();
    }
}

void FGameSettingsBackend::apply_display(FGameSettingsState const& state) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    settings->SetScreenResolution(state.resolution);
    settings->SetFullscreenMode(to_engine_window_mode(state.window_mode));
    settings->ApplyResolutionSettings(false);
}

void FGameSettingsBackend::confirm_display() const {
    if (auto* settings{user_settings()}) {
        settings->ConfirmVideoMode();
    }
}

void FGameSettingsBackend::revert_display() const {
    if (auto* settings{user_settings()}) {
        settings->RevertVideoMode();
        settings->ApplyResolutionSettings(false);
    }
}

void FGameSettingsBackend::save() const {
    if (auto* settings{user_settings()}) {
        settings->SaveSettings();
    }
}

auto FGameSettingsBackend::options(EGameSettingOptionProvider const provider) const
    -> TArray<FGameSettingOption> {
    TArray<FGameSettingOption> result;
    switch (provider) {
        case EGameSettingOptionProvider::SupportedResolutions: {
            TArray<FIntPoint> resolutions;
            UKismetSystemLibrary::GetSupportedFullscreenResolutions(resolutions);
            resolutions.Sort([](FIntPoint const& left, FIntPoint const& right) {
                return left.X == right.X ? left.Y < right.Y : left.X < right.X;
            });
            FIntPoint previous{-1, -1};
            for (auto const resolution : resolutions) {
                if (resolution == previous) {
                    continue;
                }
                previous = resolution;
                result.Add(FGameSettingOption{
                    FGameSettingValue{resolution},
                    FText::FromString(FString::Printf(TEXT("%d x %d"), resolution.X, resolution.Y)),
                });
            }
            break;
        }
        case EGameSettingOptionProvider::WindowModes: {
            result = {
                make_option(EGameWindowMode::Windowed, TEXT("Windowed")),
                make_option(EGameWindowMode::Borderless, TEXT("Borderless")),
                make_option(EGameWindowMode::Fullscreen, TEXT("Fullscreen")),
            };
            break;
        }
        case EGameSettingOptionProvider::FrameRateLimits: {
            result.Add(make_option(0.0f, TEXT("Uncapped")));
            for (auto const limit : {30, 60, 90, 120, 144, 165, 240}) {
                result.Add(FGameSettingOption{
                    FGameSettingValue{static_cast<float>(limit)},
                    FText::AsNumber(limit),
                });
            }
            break;
        }
        case EGameSettingOptionProvider::AAMethods: {
            result = {
                make_option(EGameAntiAliasingMethod::Off, TEXT("Off")),
                make_option(EGameAntiAliasingMethod::FXAA, TEXT("FXAA")),
                make_option(EGameAntiAliasingMethod::TAA, TEXT("TAA")),
                make_option(EGameAntiAliasingMethod::TSR, TEXT("TSR")),
                make_option(EGameAntiAliasingMethod::SMAA, TEXT("SMAA")),
            };
            break;
        }
        case EGameSettingOptionProvider::GraphicsPresets: {
            result = {
                make_option(EGameGraphicsPreset::Custom, TEXT("Custom")),
                make_option(EGameGraphicsPreset::Low, TEXT("Low")),
                make_option(EGameGraphicsPreset::Medium, TEXT("Medium")),
                make_option(EGameGraphicsPreset::High, TEXT("High")),
                make_option(EGameGraphicsPreset::Epic, TEXT("Epic")),
            };
            break;
        }
        case EGameSettingOptionProvider::QualityLevels: {
            result = {
                make_option(EGameQualityLevel::Low, TEXT("Low")),
                make_option(EGameQualityLevel::Medium, TEXT("Medium")),
                make_option(EGameQualityLevel::High, TEXT("High")),
                make_option(EGameQualityLevel::Epic, TEXT("Epic")),
            };
            break;
        }
        case EGameSettingOptionProvider::None: {
            break;
        }
    }
    return result;
}

auto FGameSettingsBackend::is_available(EGameSettingAvailabilityProvider const provider,
                                        FGameSettingsState const& state) const -> bool {
    switch (provider) {
        case EGameSettingAvailabilityProvider::AAQuality: {
            return state.aa_method != EGameAntiAliasingMethod::Off;
        }
        case EGameSettingAvailabilityProvider::Always: {
            return true;
        }
    }
    return true;
}

auto FGameSettingsBackend::user_settings() const -> USpaceGameUserSettings* {
    if (GEngine == nullptr) {
        UE_LOG(LogTemp, Error, TEXT("Game settings are unavailable because the engine is null"));
        return nullptr;
    }
    auto* result{Cast<USpaceGameUserSettings>(GEngine->GetGameUserSettings())};
    if (result == nullptr) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Configured GameUserSettings class is not USpaceGameUserSettings"));
    }
    return result;
}

auto FGameSettingsBackend::input_user_settings() const -> USpaceGameInputUserSettings* {
    auto* const local_player{local_player_};
    if (!IsValid(local_player)) {
        return nullptr;
    }
    auto* const subsystem{
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogTemp, Error, TEXT("Enhanced Input local-player subsystem is unavailable"));
        return nullptr;
    }
    auto* const result{Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())};
    if (!IsValid(result)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Configured Enhanced Input settings class is not "
                    "USpaceGameInputUserSettings"));
    }
    return result;
}

void FGameSettingsBackend::write_non_display(USpaceGameUserSettings& settings,
                                             FGameSettingsState const& state) const {
    settings.SetVSyncEnabled(state.vsync);
    settings.SetFrameRateLimit(state.frame_rate_limit);
    settings.SetResolutionScaleValueEx(state.resolution_scale);
    settings.set_anti_aliasing_method(state.aa_method);
    settings.SetViewDistanceQuality(static_cast<int32>(state.view_distance_quality));
    settings.SetAntiAliasingQuality(static_cast<int32>(state.aa_quality));
    settings.SetShadowQuality(static_cast<int32>(state.shadow_quality));
    settings.SetGlobalIlluminationQuality(static_cast<int32>(state.global_illumination_quality));
    settings.SetReflectionQuality(static_cast<int32>(state.reflections_quality));
    settings.SetPostProcessingQuality(static_cast<int32>(state.post_processing_quality));
    settings.SetTextureQuality(static_cast<int32>(state.texture_quality));
    settings.SetVisualEffectQuality(static_cast<int32>(state.effects_quality));
    settings.SetShadingQuality(static_cast<int32>(state.shading_quality));
    settings.set_bloom_enabled(state.bloom);
    settings.set_motion_blur_enabled(state.motion_blur);
    settings.set_master_volume(state.master_volume);
    settings.set_music_volume(state.music_volume);
    settings.set_sfx_volume(state.sfx_volume);
    settings.set_ui_volume(state.ui_volume);
    settings.set_bees(state.bees);
}

} // namespace ioj

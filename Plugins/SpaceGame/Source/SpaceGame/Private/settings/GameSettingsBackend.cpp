#include "SpaceGame/settings/GameSettingsBackend.h"

#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"

namespace ml::ioj {
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

auto make_option(FGameSettingValue value, TCHAR const* label) -> FGameSettingOption {
    return FGameSettingOption{MoveTemp(value), FText::FromString(label)};
}

} // namespace

auto FGameSettingsBackend::read() const -> FGameSettingsState {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return {};
    }

    float normalized_scale{};
    float resolution_scale{};
    float minimum_scale{};
    float maximum_scale{};
    settings->GetResolutionScaleInformationEx(
        normalized_scale, resolution_scale, minimum_scale, maximum_scale);

    return FGameSettingsState{
        .resolution = settings->GetScreenResolution(),
        .window_mode = to_game_window_mode(settings->GetFullscreenMode()),
        .vsync = settings->IsVSyncEnabled(),
        .frame_rate_limit = settings->GetFrameRateLimit(),
        .resolution_scale = resolution_scale,
        .aa_method = settings->anti_aliasing_method(),
        .aa_quality = to_quality(settings->GetAntiAliasingQuality()),
        .shadow_quality = to_quality(settings->GetShadowQuality()),
        .texture_quality = to_quality(settings->GetTextureQuality()),
        .effects_quality = to_quality(settings->GetVisualEffectQuality()),
        .reflections_quality = to_quality(settings->GetReflectionQuality()),
        .shading_quality = to_quality(settings->GetShadingQuality()),
        .master_volume = settings->master_volume(),
        .music_volume = settings->music_volume(),
        .sfx_volume = settings->sfx_volume(),
        .ui_volume = settings->ui_volume(),
        .bees = settings->bees(),
    };
}

auto FGameSettingsBackend::defaults() const -> FGameSettingsState {
    auto* defaults{NewObject<USpaceGameUserSettings>()};
    defaults->SetToDefaults();

    auto* current{user_settings()};
    auto resolution{current != nullptr ? current->GetDesktopResolution() : FIntPoint{1920, 1080}};
    if (resolution.X <= 0 || resolution.Y <= 0) {
        resolution = FIntPoint{1920, 1080};
    }

    float normalized_scale{};
    float resolution_scale{};
    float minimum_scale{};
    float maximum_scale{};
    defaults->GetResolutionScaleInformationEx(
        normalized_scale, resolution_scale, minimum_scale, maximum_scale);
    return FGameSettingsState{
        .resolution = resolution,
        .window_mode = to_game_window_mode(defaults->GetFullscreenMode()),
        .vsync = defaults->IsVSyncEnabled(),
        .frame_rate_limit = defaults->GetFrameRateLimit(),
        .resolution_scale = resolution_scale,
        .aa_method = defaults->anti_aliasing_method(),
        .aa_quality = to_quality(defaults->GetAntiAliasingQuality()),
        .shadow_quality = to_quality(defaults->GetShadowQuality()),
        .texture_quality = to_quality(defaults->GetTextureQuality()),
        .effects_quality = to_quality(defaults->GetVisualEffectQuality()),
        .reflections_quality = to_quality(defaults->GetReflectionQuality()),
        .shading_quality = to_quality(defaults->GetShadingQuality()),
        .master_volume = defaults->master_volume(),
        .music_volume = defaults->music_volume(),
        .sfx_volume = defaults->sfx_volume(),
        .ui_volume = defaults->ui_volume(),
        .bees = defaults->bees(),
    };
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
            settings->preview_master_volume();
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

void FGameSettingsBackend::write_non_display(USpaceGameUserSettings& settings,
                                             FGameSettingsState const& state) const {
    settings.SetVSyncEnabled(state.vsync);
    settings.SetFrameRateLimit(state.frame_rate_limit);
    settings.SetResolutionScaleValueEx(state.resolution_scale);
    settings.set_anti_aliasing_method(state.aa_method);
    settings.SetAntiAliasingQuality(static_cast<int32>(state.aa_quality));
    settings.SetShadowQuality(static_cast<int32>(state.shadow_quality));
    settings.SetTextureQuality(static_cast<int32>(state.texture_quality));
    settings.SetVisualEffectQuality(static_cast<int32>(state.effects_quality));
    settings.SetReflectionQuality(static_cast<int32>(state.reflections_quality));
    settings.SetShadingQuality(static_cast<int32>(state.shading_quality));
    settings.set_master_volume(state.master_volume);
    settings.set_music_volume(state.music_volume);
    settings.set_sfx_volume(state.sfx_volume);
    settings.set_ui_volume(state.ui_volume);
    settings.set_bees(state.bees);
}

} // namespace ml::ioj

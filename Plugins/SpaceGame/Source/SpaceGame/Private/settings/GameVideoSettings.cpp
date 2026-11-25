#include "SpaceGame/settings/GameVideoSettings.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"
#endif

#include "GenericPlatform/GenericWindow.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/video_settings.h>
#include <ioj/settings/window_display_settings.h>

#include "Containers/Array.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "HAL/Platform.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Math/MathFwd.h"
#include "Templates/Casts.h"

#include <expected>
#include <string>
#include <vector>

namespace ioj {
namespace {

auto to_engine_window_mode(GameWindowMode const value) -> EWindowMode::Type {
    switch (value) {
        case GameWindowMode::Fullscreen: {
            return EWindowMode::Fullscreen;
        }
        case GameWindowMode::Borderless: {
            return EWindowMode::WindowedFullscreen;
        }
        default: {
            return EWindowMode::Windowed;
        }
    }
}

} // namespace

auto FGameVideoSettings::apply_quality(VideoSettings const& state) const -> SettingsResult {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return std::unexpected{"Game user settings are unavailable"};
    }

    // Unreal can reload its bootstrap INI while validating an uninitialised settings object.
    settings->ValidateSettings();
    write_non_display(*settings, state);
    settings->ApplyNonResolutionSettings();
    return {};
}

auto FGameVideoSettings::inspect_display() const
    -> std::expected<WindowDisplaySettings, std::string> {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return inspect_game_window();
#else
    return std::expected<WindowDisplaySettings, std::string>{std::unexpect,
                                                             "PIE does not own a host window"};
#endif
}

auto FGameVideoSettings::verify_display() const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return verify_game_window();
#endif
    return {};
}

auto FGameVideoSettings::apply_display(GameSettingsState const& state) const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return preview_game_window({state.window_mode, state.resolution});
#else
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return SettingsResult{std::unexpect, "Game user settings are unavailable"};
    }

    settings->SetScreenResolution({state.resolution.width, state.resolution.height});
    settings->SetFullscreenMode(to_engine_window_mode(state.window_mode));
    settings->ApplyResolutionSettings(false);
#endif
    return {};
}

void FGameVideoSettings::confirm_display() const {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    keep_game_window();
#else
    if (auto* settings{user_settings()}) {
        settings->ConfirmVideoMode();
    }
#endif
}

auto FGameVideoSettings::revert_display() const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    return revert_game_window();
#else
    if (auto* settings{user_settings()}) {
        settings->RevertVideoMode();
        settings->ApplyResolutionSettings(false);
    }
#endif
    return {};
}

auto FGameVideoSettings::supported_resolutions(GameSettingOptionProvider provider,
                                               GameWindowMode mode) const
    -> std::vector<ScreenResolution> {
    std::vector<ScreenResolution> result;
    if (provider == GameSettingOptionProvider::SupportedResolutions) {
        TArray<FIntPoint> available;
        UKismetSystemLibrary::GetSupportedFullscreenResolutions(available);
        if (mode == GameWindowMode::Windowed) {
            TArray<FIntPoint> windowed;
            UKismetSystemLibrary::GetConvenientWindowedResolutions(windowed);
            available.Append(windowed);
        }
        result.reserve(available.Num());
        for (auto value : available) {
            result.push_back({value.X, value.Y});
        }
    }
    return result;
}

auto FGameVideoSettings::user_settings() const -> USpaceGameUserSettings* {
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

void FGameVideoSettings::write_non_display(USpaceGameUserSettings& settings,
                                           VideoSettings const& state) const {
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
}

} // namespace ioj

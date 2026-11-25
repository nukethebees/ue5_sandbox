#include "SpaceGame/settings/GameVideoSettings.h"

#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/settings/player_settings.h>
#include <ioj/settings/video_settings.h>

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Templates/Casts.h"

#include <expected>
#include <string>

namespace ioj {
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

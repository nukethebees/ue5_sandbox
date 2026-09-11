#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Components/AudioComponent.h>
#include <Kismet/GameplayStatics.h>
#include <Sound/SoundWave.h>
#include <UObject/UObjectGlobals.h>

namespace ml::ioj::game_audio {
inline constexpr TCHAR menu_ambience_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_MenuAmbience_ComputerRoomLow02."
         "A_MenuAmbience_ComputerRoomLow02")};
}

namespace ml::ioj {
void FGameAudio::initialize() {
    external_audio_ = resolve_boom_audio_library(get_configured_boom_audio_root());
    if (external_audio_.root.path.IsEmpty()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is unset. External audio sources are unavailable."));
    } else if (!external_audio_.root.valid) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is not an existing directory: \"%s\". External audio "
                    "sources are unavailable."),
               *external_audio_.root.path);
    } else {
        auto all_directories_valid{true};
        for (auto const& directory : external_audio_.directories) {
            if (directory.valid) {
                continue;
            }

            all_directories_valid = false;
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected external audio source directory is missing or is not a "
                        "directory: \"%s\" (relative directory \"%s\")."),
                   *directory.path,
                   *directory.relative_directory);
        }

        if (all_directories_valid && !external_audio_.menu_ambience_source.valid) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected menu ambience source file is missing: \"%s\" (relative path "
                        "\"%s\")."),
                   *external_audio_.menu_ambience_source.path,
                   *external_audio_.menu_ambience_source.relative_path);
        }
    }

    auto* const menu_ambience{
        LoadObject<USoundWave>(nullptr, game_audio::menu_ambience_object_path, {}, LOAD_NoWarn)};
    menu_ambience_sound_.Reset(menu_ambience);
    if (!menu_ambience_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional menu ambience asset is unavailable: \"%s\". Run `cmake "
                    "--workflow --preset import-menu-ambience` to import it locally."),
               game_audio::menu_ambience_object_path);
    }
}

void FGameAudio::start_menu_ambience(UObject const& world_context) {
    auto* const active_component{menu_ambience_component_.Get()};
    if (IsValid(active_component) && active_component->IsPlaying()) {
        return;
    }

    stop_menu_ambience();
    auto* const menu_ambience{menu_ambience_sound_.Get()};
    if (!IsValid(menu_ambience)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::SpawnSound2D(&world_context,
                                                         menu_ambience,
                                                         music_volume_,
                                                         1.0f,
                                                         0.0f,
                                                         nullptr,
                                                         persist_across_level_transition,
                                                         auto_destroy)};
    if (!IsValid(component)) {
        if (!playback_warning_logged_) {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Could not start optional menu ambience audio."));
            playback_warning_logged_ = true;
        }
        return;
    }

    menu_ambience_component_.Reset(component);
}

void FGameAudio::stop_menu_ambience() {
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }
    menu_ambience_component_.Reset();
}

void FGameAudio::set_music_volume(float const volume) {
    music_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->SetVolumeMultiplier(music_volume_);
    }
}

auto FGameAudio::external_audio_available() const noexcept -> bool {
    if (!external_audio_.root.valid || !external_audio_.menu_ambience_source.valid) {
        return false;
    }

    for (auto const& directory : external_audio_.directories) {
        if (!directory.valid) {
            return false;
        }
    }

    return true;
}
}

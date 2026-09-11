#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <Components/AudioComponent.h>
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>
#include <Sound/SoundWave.h>
#include <UObject/UObjectGlobals.h>

namespace ml::ioj::game_audio {
inline constexpr TCHAR menu_ambience_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_MenuAmbience_ComputerRoomLow02."
         "A_MenuAmbience_ComputerRoomLow02")};
inline constexpr TCHAR menu_button_pressed_object_path[]{
    TEXT("/SpaceGame/Audio/Generated/A_MenuButtonPressed.A_MenuButtonPressed")};
inline constexpr float menu_button_playback_seconds{0.5f};
}

namespace ml::ioj {
FGameAudioFacade::FGameAudioFacade(FGameAudio& audio)
    : audio_{&audio} {}

void FGameAudioFacade::play_button_pressed() const {
    if (audio_ == nullptr) {
        return;
    }
    audio_->play_button_pressed();
}

void FGameAudio::initialize(UGameInstance& game_instance) {
    game_instance_ = &game_instance;
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
        if (all_directories_valid && !external_audio_.menu_button_pressed_source.valid) {
            UE_LOG(LogSandboxAudio,
                   Warning,
                   TEXT("Expected menu button source file is missing: \"%s\" (relative path "
                        "\"%s\")."),
                   *external_audio_.menu_button_pressed_source.path,
                   *external_audio_.menu_button_pressed_source.relative_path);
        }
    }

    auto* const menu_ambience{
        LoadObject<USoundWave>(nullptr, game_audio::menu_ambience_object_path, {}, LOAD_NoWarn)};
    menu_ambience_sound_.Reset(menu_ambience);
    if (!menu_ambience_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional menu ambience asset is unavailable: \"%s\". Run `cmake "
                    "--workflow --preset import-game-audio` to import it locally."),
               game_audio::menu_ambience_object_path);
    }

    auto* const menu_button_pressed{LoadObject<USoundWave>(
        nullptr, game_audio::menu_button_pressed_object_path, {}, LOAD_NoWarn)};
    menu_button_pressed_sound_.Reset(menu_button_pressed);
    if (!menu_button_pressed_sound_.IsValid()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Optional menu button asset is unavailable: \"%s\". Run `cmake --workflow "
                    "--preset import-game-audio` to import it locally."),
               game_audio::menu_button_pressed_object_path);
    }
}

void FGameAudio::start_menu_ambience() {
    auto* const active_component{menu_ambience_component_.Get()};
    if (IsValid(active_component) && active_component->IsPlaying()) {
        return;
    }

    stop_menu_ambience();
    auto* const menu_ambience{menu_ambience_sound_.Get()};
    if (!IsValid(menu_ambience)) {
        return;
    }
    auto* const game_instance{game_instance_.Get()};
    if (!IsValid(game_instance)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::SpawnSound2D(game_instance,
                                                         menu_ambience,
                                                         music_volume_,
                                                         1.0f,
                                                         0.0f,
                                                         nullptr,
                                                         persist_across_level_transition,
                                                         auto_destroy)};
    if (!IsValid(component)) {
        if (!ambience_playback_warning_logged_) {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Could not start optional menu ambience audio."));
            ambience_playback_warning_logged_ = true;
        }
        return;
    }

    menu_ambience_component_.Reset(component);
}

void FGameAudio::play_button_pressed() {
    stop_button_audio();

    auto* const sound{menu_button_pressed_sound_.Get()};
    if (!IsValid(sound)) {
        return;
    }
    auto* const game_instance{game_instance_.Get()};
    if (!IsValid(game_instance)) {
        return;
    }

    constexpr bool persist_across_level_transition{false};
    constexpr bool auto_destroy{false};
    auto* const component{UGameplayStatics::CreateSound2D(game_instance,
                                                          sound,
                                                          sfx_volume_,
                                                          1.0f,
                                                          0.0f,
                                                          nullptr,
                                                          persist_across_level_transition,
                                                          auto_destroy)};
    if (!IsValid(component)) {
        if (!button_playback_warning_logged_) {
            UE_LOG(LogSandboxAudio, Warning, TEXT("Could not start optional menu button audio."));
            button_playback_warning_logged_ = true;
        }
        return;
    }

    menu_button_pressed_component_.Reset(component);
    component->Play(0.0f);
    component->StopDelayed(game_audio::menu_button_playback_seconds);
}

void FGameAudio::stop_menu_ambience() {
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }
    menu_ambience_component_.Reset();
}

void FGameAudio::stop_button_audio() {
    auto* const component{menu_button_pressed_component_.Get()};
    if (IsValid(component)) {
        component->Stop();
        component->DestroyComponent();
    }
    menu_button_pressed_component_.Reset();
}

void FGameAudio::set_music_volume(float const volume) {
    music_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const component{menu_ambience_component_.Get()};
    if (IsValid(component)) {
        component->SetVolumeMultiplier(music_volume_);
    }
}

void FGameAudio::set_sfx_volume(float const volume) {
    sfx_volume_ = FMath::Clamp(volume, 0.0f, 1.0f);
    auto* const component{menu_button_pressed_component_.Get()};
    if (IsValid(component)) {
        component->SetVolumeMultiplier(sfx_volume_);
    }
}

auto FGameAudio::facade() -> FGameAudioFacade {
    return FGameAudioFacade{*this};
}

auto FGameAudio::external_audio_available() const noexcept -> bool {
    if (!external_audio_.root.valid || !external_audio_.menu_ambience_source.valid ||
        !external_audio_.menu_button_pressed_source.valid) {
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

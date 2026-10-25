#pragma once

#include <ioj/presentation/boom_audio_library.h>

#include <Components/AudioComponent.h>
#include <CoreMinimal.h>
#include <Sound/SoundWave.h>
#include <UObject/StrongObjectPtr.h>
#include <UObject/WeakObjectPtr.h>

class UObject;
class UGameInstance;

namespace ioj {
struct FGameAudio;

class SPACEGAMEPRESENTATION_API FGameAudioFacade {
  public:
    FGameAudioFacade() = default;
    explicit FGameAudioFacade(FGameAudio& audio);

    void play_button_pressed() const;
  private:
    FGameAudio* audio_{};
};

struct SPACEGAMEPRESENTATION_API FGameAudio {
    void initialize(UGameInstance& game_instance);
    void start_menu_ambience();
    void stop_menu_ambience();
    void start_player_ship_ambience();
    void stop_player_ship_ambience();
    void stop_button_audio();
    void set_master_volume(float volume);
    void set_music_volume(float volume);
    void set_sfx_volume(float volume);

    [[nodiscard]] auto facade() -> FGameAudioFacade;

    [[nodiscard]] auto external_audio_available() const noexcept -> bool;
  private:
    friend FGameAudioFacade;

    void play_button_pressed();

    TWeakObjectPtr<UGameInstance> game_instance_{};
    presentation::AudioLibraryResolution external_audio_;
    TStrongObjectPtr<USoundWave> menu_ambience_sound_;
    TWeakObjectPtr<UAudioComponent> menu_ambience_component_;
    TStrongObjectPtr<USoundWave> menu_button_pressed_sound_;
    TWeakObjectPtr<UAudioComponent> menu_button_pressed_component_;
    TStrongObjectPtr<USoundWave> player_ship_ambience_sound_;
    TWeakObjectPtr<UAudioComponent> player_ship_ambience_component_;
    float master_volume_{1.0f};
    float music_volume_{1.0f};
    float sfx_volume_{1.0f};
    bool ambience_playback_warning_logged_{};
    bool button_playback_warning_logged_{};
    bool player_ship_ambience_playback_warning_logged_{};
};
}

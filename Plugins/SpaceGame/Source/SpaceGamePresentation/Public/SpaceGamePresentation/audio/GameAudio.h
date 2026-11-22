#pragma once

#include <ioj/presentation/audio_source_library.h>

#include <Components/AudioComponent.h>
#include <CoreMinimal.h>
#include <Sound/SoundAttenuation.h>
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
    void play_player_laser() const;
    void play_entity_death(FVector location) const;
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
    void play_player_laser();
    void play_entity_death(FVector location);

    TWeakObjectPtr<UGameInstance> game_instance_{};
    presentation::AudioLibraryResolution external_audio_;
    TArray<TStrongObjectPtr<USoundWave>> death_impact_sounds_;
    TStrongObjectPtr<USoundAttenuation> death_attenuation_;
    std::array<std::uint8_t, 10'000> death_impact_indices_{};
    std::size_t next_impact_{};
    TStrongObjectPtr<USoundWave> player_laser_sound_;
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

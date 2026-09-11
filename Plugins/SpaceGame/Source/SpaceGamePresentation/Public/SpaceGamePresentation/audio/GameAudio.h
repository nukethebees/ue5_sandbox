#pragma once

#include <Components/AudioComponent.h>
#include <CoreMinimal.h>
#include <Sound/SoundWave.h>
#include <UObject/StrongObjectPtr.h>

class UObject;

namespace ml::ioj {
struct SPACEGAMEPRESENTATION_API FAudioSourceRoot {
    FString path;
    bool valid{};
};

struct SPACEGAMEPRESENTATION_API FAudioSourceDirectory {
    FString relative_directory;
    FString path;
    bool valid{};
};

struct SPACEGAMEPRESENTATION_API FAudioSourceFile {
    FString relative_path;
    FString path;
    bool valid{};
};

struct SPACEGAMEPRESENTATION_API FExternalAudioLibraryResolution {
    FAudioSourceRoot root;
    TArray<FAudioSourceDirectory> directories;
    FAudioSourceFile menu_ambience_source;
};

struct SPACEGAMEPRESENTATION_API FGameAudio {
    void initialize();
    void start_menu_ambience(UObject const& world_context);
    void stop_menu_ambience();
    void set_music_volume(float volume);

    [[nodiscard]] auto external_audio_available() const noexcept -> bool;
  private:
    FExternalAudioLibraryResolution external_audio_;
    TStrongObjectPtr<USoundWave> menu_ambience_sound_;
    TStrongObjectPtr<UAudioComponent> menu_ambience_component_;
    float music_volume_{1.0f};
    bool playback_warning_logged_{};
};
}

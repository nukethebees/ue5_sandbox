#pragma once

#include <CoreMinimal.h>

namespace ml::ioj {
struct SPACEGAMEPRESENTATION_API FAudioSourceDirectory {
    FString relative_directory;
    FString path;
    bool valid{};
};

struct SPACEGAMEPRESENTATION_API FAudioSourceRoot {
    FString path;
    bool valid{};
    TArray<FAudioSourceDirectory> directories;
};

struct SPACEGAMEPRESENTATION_API FGameAudio {
    void initialize();

    [[nodiscard]] auto external_audio_available() const noexcept -> bool;
  private:
    FAudioSourceRoot external_audio_;
};
}

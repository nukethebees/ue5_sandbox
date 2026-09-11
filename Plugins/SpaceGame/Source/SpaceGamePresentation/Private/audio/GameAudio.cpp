#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

namespace ml::ioj {
void FGameAudio::initialize() {
    external_audio_ = resolve_boom_audio_library(get_configured_boom_audio_root());
    if (external_audio_.path.IsEmpty()) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is unset. External audio sources are unavailable."));
        return;
    }
    if (!external_audio_.valid) {
        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("BEE_AUDIO_ROOT is not an existing directory: \"%s\". External audio "
                    "sources are unavailable."),
               *external_audio_.path);
        return;
    }

    for (auto const& directory : external_audio_.directories) {
        if (directory.valid) {
            continue;
        }

        UE_LOG(LogSandboxAudio,
               Warning,
               TEXT("Expected external audio source directory is missing or is not a directory: "
                    "\"%s\" (relative directory \"%s\")."),
               *directory.path,
               *directory.relative_directory);
    }
}

auto FGameAudio::external_audio_available() const noexcept -> bool {
    if (!external_audio_.valid) {
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

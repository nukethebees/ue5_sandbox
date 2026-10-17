#pragma once

#include <SpaceGamePresentation/audio/GameAudio.h>

namespace ioj {
SPACEGAMEPRESENTATION_API auto get_configured_boom_audio_root() -> FString;
SPACEGAMEPRESENTATION_API auto resolve_boom_audio_library(FStringView configured_root)
    -> FExternalAudioLibraryResolution;
}

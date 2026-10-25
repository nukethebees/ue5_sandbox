#pragma once

#include <ioj/presentation/boom_audio_library.h>

#include <Containers/UnrealString.h>

namespace ioj {
SPACEGAMEPRESENTATION_API auto get_configured_boom_audio_root() -> FString;
SPACEGAMEPRESENTATION_API auto resolve_boom_audio_library(FStringView configured_root)
    -> presentation::AudioLibraryResolution;
}

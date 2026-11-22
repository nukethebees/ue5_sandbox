#pragma once

#include <ioj/presentation/audio_source_library.h>

#include <Containers/UnrealString.h>

namespace ioj {
SPACEGAMEPRESENTATION_API auto get_configured_audio_root() -> FString;
SPACEGAMEPRESENTATION_API auto resolve_audio_source_library(FStringView configured_root)
    -> presentation::AudioLibraryResolution;
}

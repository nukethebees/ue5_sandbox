#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>

#include <HAL/PlatformMisc.h>
#include <Misc/Paths.h>

auto ioj::get_configured_boom_audio_root() -> FString {
    return FPlatformMisc::GetEnvironmentVariable(TEXT("BEE_AUDIO_ROOT"));
}

auto ioj::resolve_boom_audio_library(FStringView const configured_root)
    -> presentation::AudioLibraryResolution {
    if (configured_root.IsEmpty()) {
        return {};
    }
    auto path{FPaths::ConvertRelativePathToFull(FString{configured_root})};
    FPaths::NormalizeDirectoryName(path);
    return presentation::resolve_boom_audio_library(std::filesystem::path{*path});
}

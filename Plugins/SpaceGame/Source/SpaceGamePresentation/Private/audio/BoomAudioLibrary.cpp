#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>

#include <HAL/FileManager.h>
#include <HAL/PlatformMisc.h>
#include <Misc/Paths.h>

namespace ml::ioj::boom_audio_library {
inline constexpr TCHAR root_environment_variable[]{TEXT("BEE_AUDIO_ROOT")};

inline constexpr TCHAR sci_fi_designed_directory[]{TEXT("sci-fi_ds_2220mb")};

inline constexpr TCHAR const* pack_directories[]{
    sci_fi_designed_directory,
};
}

auto ml::ioj::get_configured_boom_audio_root() -> FString {
    return FPlatformMisc::GetEnvironmentVariable(boom_audio_library::root_environment_variable);
}

auto ml::ioj::resolve_boom_audio_library(FStringView const configured_root) -> FAudioSourceRoot {
    FAudioSourceRoot root;
    if (configured_root.IsEmpty()) {
        return root;
    }

    root.path = FPaths::ConvertRelativePathToFull(FString{configured_root});
    FPaths::NormalizeDirectoryName(root.path);

    auto& file_manager{IFileManager::Get()};
    if (!file_manager.DirectoryExists(*root.path)) {
        return root;
    }

    root.valid = true;
    for (auto const* const relative_directory : boom_audio_library::pack_directories) {
        auto path{FPaths::Combine(root.path, relative_directory)};
        root.directories.Emplace(FAudioSourceDirectory{
            relative_directory,
            path,
            file_manager.DirectoryExists(*path),
        });
    }

    return root;
}

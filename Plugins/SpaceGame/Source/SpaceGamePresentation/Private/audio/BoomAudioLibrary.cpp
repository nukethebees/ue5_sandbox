#include <SpaceGamePresentation/audio/BoomAudioLibrary.h>

#include <HAL/FileManager.h>
#include <HAL/PlatformMisc.h>
#include <Misc/Paths.h>

namespace ml::ioj::boom_audio_library {
inline constexpr TCHAR root_environment_variable[]{TEXT("BEE_AUDIO_ROOT")};

inline constexpr TCHAR sci_fi_designed_directory[]{TEXT("sci-fi_ds_2220mb")};
inline constexpr TCHAR menu_ambience_filename[]{
    TEXT("AMBDsgn_Ambience Computer Room Low 02_B00M_SFDS.wav")};
inline constexpr TCHAR menu_button_pressed_filename[]{
    TEXT("SCICmpt_Computer Beep High 01_B00M_SFDS.wav")};
inline constexpr TCHAR player_ship_ambience_filename[]{
    TEXT("AMBDsgn_Ambience Spaceship High 02_B00M_SFDS.wav")};

inline constexpr TCHAR const* pack_directories[]{
    sci_fi_designed_directory,
};
}

auto ml::ioj::get_configured_boom_audio_root() -> FString {
    return FPlatformMisc::GetEnvironmentVariable(boom_audio_library::root_environment_variable);
}

auto ml::ioj::resolve_boom_audio_library(FStringView const configured_root)
    -> FExternalAudioLibraryResolution {
    FExternalAudioLibraryResolution resolution;
    if (configured_root.IsEmpty()) {
        return resolution;
    }

    resolution.root.path = FPaths::ConvertRelativePathToFull(FString{configured_root});
    FPaths::NormalizeDirectoryName(resolution.root.path);

    auto& file_manager{IFileManager::Get()};
    if (!file_manager.DirectoryExists(*resolution.root.path)) {
        return resolution;
    }

    resolution.root.valid = true;
    for (auto const* const relative_directory : boom_audio_library::pack_directories) {
        auto path{FPaths::Combine(resolution.root.path, relative_directory)};
        resolution.directories.Emplace(FAudioSourceDirectory{
            relative_directory,
            path,
            file_manager.DirectoryExists(*path),
        });
    }

    auto const relative_path{FPaths::Combine(boom_audio_library::sci_fi_designed_directory,
                                             boom_audio_library::menu_ambience_filename)};
    auto const source_path{FPaths::Combine(resolution.root.path, relative_path)};
    resolution.menu_ambience_source = FAudioSourceFile{
        relative_path,
        source_path,
        file_manager.FileExists(*source_path),
    };

    auto const button_relative_path{
        FPaths::Combine(boom_audio_library::sci_fi_designed_directory,
                        boom_audio_library::menu_button_pressed_filename)};
    auto const button_source_path{FPaths::Combine(resolution.root.path, button_relative_path)};
    resolution.menu_button_pressed_source = FAudioSourceFile{
        button_relative_path,
        button_source_path,
        file_manager.FileExists(*button_source_path),
    };

    auto const player_ship_ambience_relative_path{
        FPaths::Combine(boom_audio_library::sci_fi_designed_directory,
                        boom_audio_library::player_ship_ambience_filename)};
    auto const player_ship_ambience_source_path{
        FPaths::Combine(resolution.root.path, player_ship_ambience_relative_path)};
    resolution.player_ship_ambience_source = FAudioSourceFile{
        player_ship_ambience_relative_path,
        player_ship_ambience_source_path,
        file_manager.FileExists(*player_ship_ambience_source_path),
    };

    return resolution;
}

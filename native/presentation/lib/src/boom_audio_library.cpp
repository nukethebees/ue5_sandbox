#include <ioj/presentation/boom_audio_library.h>

namespace ioj::presentation {
auto resolve_boom_audio_library(std::filesystem::path const& configured_root)
    -> AudioLibraryResolution {
    AudioLibraryResolution result;
    if (configured_root.empty()) {
        return result;
    }

    std::error_code error;
    result.root.path = std::filesystem::absolute(configured_root, error).lexically_normal();
    if (error || !std::filesystem::is_directory(result.root.path, error)) {
        return result;
    }

    result.root.valid = true;
    std::filesystem::path const pack{"sci-fi_ds_2220mb"};
    auto const pack_path{result.root.path / pack};
    result.directories.push_back(
        {pack, pack_path, std::filesystem::is_directory(pack_path, error)});
    auto const source{[&](char const* filename) -> AudioSource {
        auto const relative{pack / filename};
        auto const path{result.root.path / relative};
        return {relative, path, std::filesystem::is_regular_file(path, error)};
    }};
    result.menu_ambience_source = source("AMBDsgn_Ambience Computer Room Low 02_B00M_SFDS.wav");
    result.menu_button_pressed_source = source("SCICmpt_Computer Beep High 01_B00M_SFDS.wav");
    result.player_ship_ambience_source = source("AMBDsgn_Ambience Spaceship High 02_B00M_SFDS.wav");
    return result;
}
}

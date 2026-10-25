#pragma once

#include <filesystem>
#include <vector>

namespace ioj::presentation {
struct AudioSourceRoot {
    std::filesystem::path path;
    bool valid{};
};

struct AudioSource {
    std::filesystem::path relative_path;
    std::filesystem::path path;
    bool valid{};
};

struct AudioLibraryResolution {
    AudioSourceRoot root;
    std::vector<AudioSource> directories;
    AudioSource menu_ambience_source;
    AudioSource menu_button_pressed_source;
    AudioSource player_ship_ambience_source;
};

auto resolve_boom_audio_library(std::filesystem::path const& configured_root)
    -> AudioLibraryResolution;
}

#pragma once

#include <string>

namespace ioj::sim {
// Attach level identity and persistence policy to the player's completed run.
struct LevelResultMetadata {
    std::string level_id{};
    std::string level_title{};
    bool save_results{true};
};
}

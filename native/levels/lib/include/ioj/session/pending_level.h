#pragma once

#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_source.h>

#include <string>

namespace ioj::session {
struct PendingLevel {
    levels::LevelSource source;
    std::string source_path{};
    levels::LaunchOptions options{};
};
}

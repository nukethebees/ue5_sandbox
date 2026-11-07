#pragma once

#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_definition.h>

#include <string>

namespace ioj::session {
struct PendingLevel {
    levels::LevelDefinition definition;
    std::string source_path{};
    levels::LaunchOptions options{};
};
}

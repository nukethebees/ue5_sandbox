#pragma once

#include <ioj/levels/level_definition.h>

#include <string>

namespace ioj::levels::authoring {
struct SourceReplay {
    LevelDefinition definition;
    std::string evaluated_source;
};
}
